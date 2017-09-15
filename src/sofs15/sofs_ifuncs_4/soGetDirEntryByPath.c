#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>
#include "sofs_probe.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_direntry.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_1.h"
#include "sofs_ifuncs_2.h"
#include "sofs_ifuncs_3.h"
 
/* Allusion to external function */
int soGetDirEntryByName (uint32_t nInodeDir, const char *eName, uint32_t *p_nInodeEnt, uint32_t *p_idx);
/* Allusion to internal function */
 
static int soTraversePath (const char *ePath, uint32_t *p_nInodeDir, uint32_t *p_nInodeEnt);
/** \brief Number of symbolic links in the path */
 
static uint32_t nSymLinks = 0;
/** \brief Old directory inode number */
 
//static uint32_t oldNInodeDir = 0;
 
/**
 *  \brief Get an entry by path.
 *
 *  The directory hierarchy of the file system is traversed to find an entry whose name is the rightmost component of
 *  <tt>ePath</tt>. The path is supposed to be absolute and each component of <tt>ePath</tt>, with the exception of the
 *  rightmost one, should be a directory name or symbolic link name to a path.
 *
 *  The process that calls the operation must have execution (x) permission on all the components of the path with
 *  exception of the rightmost one.
 *
 *  \param ePath pointer to the string holding the name of the path
 *  \param p_nInodeDir pointer to the location where the number of the inode associated to the directory that holds the
 *                     entry is to be stored
 *                     (nothing is stored if \c NULL)
 *  \param p_nInodeEnt pointer to the location where the number of the inode associated to the entry is to be stored
 *                     (nothing is stored if \c NULL)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL
 *  \return -\c ENAMETOOLONG, if the path or any of the path components exceed the maximum allowed length
 *  \return -\c ERELPATH, if the path is relative and it is not a symbolic link
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT,  if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EDIRINVAL, if the directory is inconsistent
 *  \return -\c EDEINVAL, if the directory entry is inconsistent
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
 
int soGetDirEntryByPath (const char *ePath, uint32_t *p_nInodeDir, uint32_t *p_nInodeEnt)
{
  soColorProbe (311, "07;31", "soGetDirEntryByPath (\"%s\", %p, %p)\n", ePath, p_nInodeDir, p_nInodeDir);
  
  int status;
  uint32_t nInodeDir; // father inode number (directory)
  uint32_t nInodeEnt; // entry inode number
 
  if(ePath == NULL) return -EINVAL;
  if(strlen(ePath) == 0) return -EINVAL;
  if(strlen(ePath) > MAX_PATH) return -ENAMETOOLONG;
  if(ePath[0] != '/') return -ERELPATH; // check if ePath is a absolte path
 
  // go all directory
  if((status = soTraversePath(ePath,&nInodeDir,&nInodeEnt))!=0) return status;
 
  // return the necessaries inode numbers
  if(p_nInodeDir!=NULL) *p_nInodeDir = nInodeDir;
  if(p_nInodeEnt!=NULL) *p_nInodeEnt = nInodeEnt;
 
  return 0;
}
 
/**
 *  \brief Traverse the path.
 *
 *  \param ePath pointer to the string holding the name of the path
 *  \param p_nInodeDir pointer to the location where the number of the inode associated to the directory that holds the
 *                     entry is to be stored
 *  \param p_nInodeEnt pointer to the location where the number of the inode associated to the entry is to be stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c ENAMETOOLONG, if any of the path components exceed the maximum allowed length
 *  \return -\c ERELPATH, if the path is relative and it is not a symbolic link
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT,  if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EDIRINVAL, if the directory is inconsistent
 *  \return -\c EDEINVAL, if the directory entry is inconsistent
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
 
static int soTraversePath (const char *ePath, uint32_t *p_nInodeDir, uint32_t *p_nInodeEnt)
{
    char path[ MAX_PATH + 1 ];  // dirname of the path
    char name[ MAX_PATH + 1 ];  // basename of the path
    char data[ BSLPC ];        
    int status;                
    SOInode inode;              
    uint32_t nInodeDir;        
    uint32_t nInodeEnt;             
 
    // copy content of ePath to name and path variables
    strcpy( name, ePath );
    strcpy( path, ePath );
 
    // Executes  basename e o dirname of string ePath
    strcpy(name, basename(name) );
    strcpy(path, dirname(path) );
 
    // if root directory returns inode(0)
    if (strcmp(path,"/") == 0)      
    {
        if (strcmp(name,"/") == 0)      
            strcpy(name,".");     
        
        nInodeDir = 0;  // inode of root directory

        // read dir inode
        if ((status = soReadInode(&inode, nInodeDir)) != 0) return status;
        // checks if it is a dir
        if ((inode.mode & INODE_DIR) != INODE_DIR) return -ENOTDIR;
        // checks if it is possible to read the dir
        if ((status = soAccessGranted(nInodeDir, X)) !=0) return status;
        // call function to return inode of entry (basename)
        if ((status = soGetDirEntryByName(nInodeDir, name, &nInodeEnt, NULL)) != 0) return status;
 
    }
 
    else                
    {
        if ((status = soTraversePath(path,&nInodeDir, &nInodeEnt)) != 0) return status;
        nInodeDir = nInodeEnt;
 
        // read dir inode
        if ((status = soReadInode(&inode, nInodeDir)) != 0) return status;
        // checks if it is a dir
        if ((inode.mode & INODE_DIR) != INODE_DIR) return -ENOTDIR;
        // checks if it is possible to read the dir
        if ((status = soAccessGranted(nInodeDir, X)) !=0) return status;
        // call function to return inode of entry (basename)
        if ((status = soGetDirEntryByName(nInodeDir, name, &nInodeEnt, NULL)) != 0) return status;
        // read basename inode
        if ((status = soReadInode(&inode, nInodeEnt)) != 0) return status;
    }
 
    /*************   Checks for SHORTCUT (atalho)   ***************************/
    // reads no-i and checks if is shortcut (symlink)
    if ((status = soReadInode(&inode,nInodeEnt)) != 0) return status;  
 
    // if not shortcut TERMINATE!!
    if ((inode.mode & INODE_SYMLINK) != (INODE_SYMLINK))    
    {
        // return number to the no-i of father directory, if necessary
        if ( p_nInodeDir != NULL)  *p_nInodeDir = nInodeDir;    
 
        // return number to the no-i of entry(entrada), if necessary
        if ( p_nInodeEnt != NULL)  *p_nInodeEnt = nInodeEnt;
        return 0;
    }
 
    else
    {   // Checks if shortcut found is not the first
        if (nSymLinks>=1)    
        {
            nSymLinks=0;
            return -ELOOP;
        }
    
        // checks if it is possible to read and execute the symlink
        if ((status = soAccessGranted(nInodeEnt, R+X)) != 0) { return status;}
        // reads content of Cluster that belongs to the shortcut
        if ((status = soReadFileCluster(nInodeEnt, 0, data)) != 0) return status;
 
        if (data[0] != '/')     // if the path is the relative path
        {
            /** Checks if last caracter of path is '/', or else puts '/' in the end
                then join the relative path with the given path **/
 
            if (path[strlen(path)-1] != '/')    
                strncat(path,"/",MAX_PATH+1);  
 
            // Join the shortcut to the absolut path of father directory determined before
            strncat(path,data,MAX_PATH+1);      
        }
        else
            strncpy(path,data,MAX_PATH+1);  // if its absolut the new path its entirely given by the shortcut

        nSymLinks = 1;
 
        // recursive -> with the new shortcut path
        if ((status = soTraversePath(path, &nInodeDir, &nInodeEnt)) != 0) return status;    
 
        // real end of recursive function, reset shortcut counter
        nSymLinks = 0;
 
    }
 
    // return the necessaries inode numbers
    // return number to the no-i of father directory, if necessary
    if ( p_nInodeDir != NULL)  *p_nInodeDir = nInodeDir;
    // return number to the no-i of entry(entrada), if necessary
    if ( p_nInodeEnt != NULL)  *p_nInodeEnt = nInodeEnt;
    return 0;
}