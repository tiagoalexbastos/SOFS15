/**
 *  \file soRename.c (implementation file)
 *
 *  \author Daniela Sousa, 70158
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <libgen.h>
#include <string.h>

#include "sofs_probe.h"
#include "sofs_const.h"
#include "sofs_rawdisk.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_direntry.h"
#include "sofs_datacluster.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_1.h"
#include "sofs_ifuncs_2.h"
#include "sofs_ifuncs_3.h"
#include "sofs_ifuncs_4.h"

/**
 *  \brief Change the name or the location of a file in the directory hierarchy of the file system.
 *
 *  It tries to emulate <em>rename</em> system call.
 *
 *  \param old path to an existing file
 *  \param new new path to the same file in replacement of the old one
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to either of the strings is \c NULL or any of the path strings is a \c NULL string
 *                      or any of the paths do not describe absolute paths or <tt>old</tt> describes a directory
 *                      and is a substring of <tt>new</tt> (attempt to make a directory a subdirectory of itself)
 *  \return -\c ENAMETOOLONG, if the paths names or any of their components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of both paths, but the last one, are not directories, or
 *                       <tt>old</tt> describes a directory and <tt>new</tt>, although it exists, does not
 *  \return -\c EISDIR, if <tt>new</tt> describes a directory and <tt>old</tt> does not
 *  \return -\c ELOOP, if either path resolves to more than one symbolic link
 *  \return -\c EMLINK, if <tt>old</tt> is a directory and the directory containing <tt>new</tt> has already
 *                      the maximum number of links, or <tt>old</tt> has already the maximum number of links and
 *                      is not contained in the same directory that will contain <tt>new</tt>
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>old</tt>, or to any of the
 *                      components of <tt>new</tt>, but the last one, is found
 *  \return -\c ENOTEMPTY, if both <tt>old</tt> and <tt>new</tt> describe directories and <tt>new</tt> is
 *                         not empty
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of both paths, but the last one
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the directories where
 *                     <tt>new</tt> entry is to be added and <tt>old</tt> is to be detached
 *  \return -\c ENOSPC, if there are no free data clusters
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soRename (const char *oldPath, const char *newPath)
{
  soColorProbe (227, "07;31", "soRename (\"%s\", \"%s\")\n", oldPath, newPath);


    int stat, stat2; // function status return control
    uint32_t nInodeOld_dir, nInodeOld_ent; 
    uint32_t nInodeNew_dir, nInodeNew_dir_dir, nInodeNew_ent; 
    SOInode oldinode, newinode, base_newinode; 
    uint32_t op;                    // ADD/ATTACH 
    // new strings to avoid compiler warnings
    char old[MAX_PATH + 1]; 
    char new[MAX_PATH + 1]; 
    char basenameOld[MAX_NAME + 1]; 
    char basenameNew[MAX_NAME + 1]; 
    char dirnameNew[MAX_PATH + 1]; 

      if (oldPath == NULL || newPath == NULL) return -EINVAL;

    if ((strlen(oldPath) == 0) || (strlen(newPath) == 0)) return -EINVAL; 

    if (strlen(oldPath) > MAX_PATH) return -ENAMETOOLONG;

    if (strlen(newPath) > MAX_PATH) return -ENAMETOOLONG;

    // avoid compiler warning "passing argument 1 of ‘__xpg_basename’ discards ‘const’ qualifier from pointer target type" for basename
    // (what if at this point basename of arguments is longer than MAX_NAME+1 ?)
    strcpy(old, oldPath);
    strcpy(basenameOld, basename(old));
    strcpy(new, newPath);
    strcpy(basenameNew, basename(new));
    strcpy(dirnameNew, dirname(new));

    

    if ((stat = soGetDirEntryByPath(oldPath, &nInodeOld_dir, &nInodeOld_ent)) != 0)
        return stat;

    if ((stat = soReadInode(&oldinode, nInodeOld_ent)) != 0)
        return stat;

    bool newExists;
    if ((stat = soGetDirEntryByPath(newPath, &nInodeNew_dir, &nInodeNew_ent)) != 0) {
        if (stat == -ENOENT) 
            newExists= false;
        else
            return stat;
    }else
        newExists= true;

//-------------------------------------------------------------situação 1 e 3
   
    if (newExists==false){



            // read dirname of existing target, fill it's inode dir and inode entry
            if ((stat = soGetDirEntryByPath(dirnameNew, &nInodeNew_dir_dir, &nInodeNew_dir)) != 0)
                return stat;

            if ((stat = soReadInode(&newinode, nInodeNew_dir)) != 0) 
                return stat;
            


            //-----------------------situação 1----------------------------------------------------

            //name alteration:
            if (nInodeOld_dir == nInodeNew_dir) { 
                
                if ((stat = soRenameDirEntry(nInodeOld_dir, basenameOld, basenameNew)) != 0)
                    return stat;

                return 0;
            } 
            
            //--------------------------situaçao 3--------------------------------------------------

            else { //dir location alteration:

                // new doesn't exist 
                //nInodeOld_dir != nInodeNew_dir
       


                if ((oldinode.mode & INODE_DIR) == INODE_DIR){
                    if ((stat = soAddAttDirEntry(nInodeNew_dir, basenameNew, nInodeOld_ent, ATTACH)) != 0)
                        return stat;

                    if ((stat = soRemDetachDirEntry(nInodeOld_dir, basenameOld, DETACH)) != 0)//detach da entry no old path
                        return stat;
                    return 0;
                }
                else{

                    if ((stat = soAddAttDirEntry(nInodeNew_dir, basenameNew, nInodeOld_ent, ADD)) != 0)
                        return stat;

                    if ((stat = soRemDetachDirEntry(nInodeOld_dir, basenameOld, DETACH)) != 0)//detach da entry no old path
                        return stat;

                    return 0;
                }
                

        }//------------------------------------------end 3
    }
    
    //-------------------------------------------------------------------------------------------------------------------------------------------------

    
    if ((stat = soReadInode(&base_newinode, nInodeNew_ent)) != 0)
        return stat;

    //-----------------------------------situaçao 2 e 4

    if (nInodeOld_dir == nInodeNew_dir) { 

        if ( ((oldinode.mode & INODE_DIR) == INODE_DIR) && ((base_newinode.mode & INODE_DIR) == INODE_DIR) ){
            //se new Path entry é directorio 

            // new must be empty
            if ((stat = soCheckDirectoryEmptiness(nInodeNew_ent)) != 0)
                return stat;

            op = ATTACH;
            
                
        }else if ( (  ((oldinode.mode & INODE_FILE) == INODE_FILE) || ((oldinode.mode & INODE_SYMLINK) == INODE_SYMLINK) ) 
                            &&( ((base_newinode.mode & INODE_FILE) == INODE_FILE) || ((base_newinode.mode & INODE_SYMLINK) == INODE_SYMLINK) ) ){
            op = ADD;
            
            }
        else
            return -EISDIR;



        if ((stat = soRemDetachDirEntry(nInodeNew_dir, basenameNew, DETACH)) != 0){ 


            return stat;
        }



        if ((stat = soRenameDirEntry(nInodeOld_dir, basenameOld, basenameNew)) != 0){

            if ((stat2 = soAddAttDirEntry(nInodeNew_dir, basenameNew, nInodeNew_ent, op)) != 0){
                
                return stat2;
            }
            return stat;
        }
        return 0;
    //------------------------------------------------------------------
    }
    else{
    //---------------------------------situaçao 4

        if ( ((oldinode.mode & INODE_DIR) == INODE_DIR) && ((base_newinode.mode & INODE_DIR) == INODE_DIR) ){
            //se new Path entry é directorio 

            // new must be empty
            if ((stat = soCheckDirectoryEmptiness(nInodeNew_ent)) != 0)
                return stat;

            op = ATTACH;
            
                
        }else if ( (  ((oldinode.mode & INODE_FILE) == INODE_FILE) || ((oldinode.mode & INODE_SYMLINK) == INODE_SYMLINK) ) 
                            &&( ((base_newinode.mode & INODE_FILE) == INODE_FILE) || ((base_newinode.mode & INODE_SYMLINK) == INODE_SYMLINK) ) )
            {
            op = ADD;
            
            }
           
        else
            return -EISDIR;


        if ((stat = soRemDetachDirEntry(nInodeNew_dir, basenameNew, DETACH)) != 0)
            return stat;
        // attach old to new
        
        if ((stat = soAddAttDirEntry(nInodeNew_dir, basenameNew, nInodeOld_ent, op)) != 0){
            if ((stat2 = soAddAttDirEntry(nInodeNew_dir, basenameNew, nInodeNew_ent, op)) != 0)
                return stat2;
            return stat;
        }
        

        if ((stat = soRemDetachDirEntry(nInodeOld_dir, basenameOld, DETACH)) != 0){
            
            if ((stat2 = soRemDetachDirEntry(nInodeNew_dir, basenameNew, DETACH)) != 0)
                return stat2;
            
            
            if ((stat2 = soAddAttDirEntry(nInodeNew_dir, basenameNew, nInodeNew_ent, op)) != 0)
                return stat2;
            

            return stat;
        }

        return 0;

    }

  return 0;
}
