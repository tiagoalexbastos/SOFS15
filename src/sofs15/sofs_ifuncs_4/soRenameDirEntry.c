/**
 *  \file soRenameDirEntry.c (implementation file)
 *
 *  \author Daniela Sousa, 70158
 */

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

/* Allusion to external functions */

int soGetDirEntryByName (uint32_t nInodeDir, const char *eName, uint32_t *p_nInodeEnt, uint32_t *p_idx);

/**
 *  \brief Rename an entry of a directory.
 *
 *  The directory entry whose name is <tt>oldName</tt> has its <em>name</em> field changed to <tt>newName</tt>. Thus,
 *  the inode associated to the directory must be in use and belong to the directory type.
 *
 *  Both the <tt>oldName</tt> and the <tt>newName</tt> must be <em>base names</em> and not <em>paths</em>, that is,
 *  they can not contain the character '/'. Besides an entry whose <em>name</em> field is <tt>oldName</tt> should exist
 *  in the directory and there should not be any entry in the directory whose <em>name</em> field is <tt>newName</tt>.
 *
 *  The process that calls the operation must have write (w) and execution (x) permissions on the directory.
 *
 *  \param nInodeDir number of the inode associated to the directory
 *  \param oldName pointer to the string holding the name of the direntry to be renamed
 *  \param newName pointer to the string holding the new name
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range or either one of the pointers to the strings are
 *                      \c NULL or the name strings do not describe file names
 *  \return -\c ENAMETOOLONG, if one of the name strings exceeds the maximum allowed length
 *  \return -\c ENOTDIR, if the inode type is not a directory
 *  \return -\c ENOENT,  if no entry with <tt>oldName</tt> is found
 *  \return -\c EEXIST,  if an entry with the <tt>newName</tt> already exists
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on the directory
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the directory
 *  \return -\c EDIRINVAL, if the directory is inconsistent
 *  \return -\c EDEINVAL, if the directory entry is inconsistent
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soRenameDirEntry (uint32_t nInodeDir, const char *oldName, const char *newName)
{
  soColorProbe (315, "07;31", "soRenameDirEntry (%"PRIu32", \"%s\", \"%s\")\n", nInodeDir, oldName, newName);

  /* insert your code here */
	SOInode Inode;                  // used to store inode to be read
    SODirEntry DirEntrys[DPC];          // used to store the array of directory entrys
    int status;                 // status for returning errors or success
    uint32_t index;                 // index of Directory Entry of Old Name dir     
     
    //check if the names are null
    if(oldName == NULL || newName == NULL)
        return -EINVAL;
     
    //check if the names are the '.' or '..'
    if(( strcmp( oldName, ".") == 0) || ( strcmp( oldName, "..") == 0) 
    || ( strcmp( newName, ".") == 0) || ( strcmp( newName, "..") == 0))
        return -EINVAL;
 
    //check if oldName or newName are a basenames
    if ( strchr(oldName, '/')!= NULL || strchr(newName, '/')!= NULL)
        return -EINVAL;
 
    //check if the length of the names arent too long
    if( (strlen(oldName) > MAX_NAME) ||  (strlen(newName) > MAX_NAME))
            return -ENAMETOOLONG;
     
    //get the inode and check if it is in use
    if((status = soReadInode(&Inode, nInodeDir)) < 0)
        return status;
     
    //check if the inode is directory
    if((Inode.mode & INODE_DIR)!=INODE_DIR) 
        return -ENOTDIR;
         
    //check if inode has allocated clusters
    if(Inode.clucount == 0)
        return -ELIBBAD;
     
    //check the inode permissions
    if( soAccessGranted( nInodeDir, X)) //successs == 0 
        return -EACCES;
     
    if( soAccessGranted( nInodeDir, W))
        return -EPERM;
 
    //get pointer to the old directory index if oldName exists
    ;
    if((status = soGetDirEntryByName(nInodeDir, oldName, NULL, &index)) !=0)
        return -ENOENT;
 
    //check if newName already exists
    
    if((status = soGetDirEntryByName(nInodeDir, newName, NULL, NULL)) == 0)
        return -EEXIST; //if exists
    if(status != -ENOENT) //if does not exist and status !=:
        return status;
 
    //read the cluster that contains the oldName
    if((status == soReadFileCluster( nInodeDir, (index/DPC), DirEntrys)) != 0) //DPC = number of directory entries per data cluste
        return status;
     
    //replace the oldName by the newName
    strcpy((char *)DirEntrys[index].name, newName);
    memset(&DirEntrys[index].name[strlen(newName)], '\0', MAX_NAME - strlen(newName) - 1);  
     
    //write the cluster with the newName
    if((status == soWriteFileCluster( nInodeDir, (index/DPC), DirEntrys)) != 0)
        return status;
 
  
  return 0;
}
