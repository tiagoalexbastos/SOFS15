/**
 *  \file soTruncate.c (implementation file)
 *
 *  \author ---
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
 *  \brief Truncate a regular file to a specified length.
 *
 *  It tries to emulate <em>truncate</em> system call.
 *
 *  \param ePath path to the file
 *  \param length new size for the regular size
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or or the path string is a \c NULL string or the path does
 *                      not describe an absolute path
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c EISDIR, if <tt>ePath</tt> describes a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EFBIG, if the file may grow passing its maximum size
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the file described by
 *                     <tt>ePath</tt>
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soTruncate (const char *ePath, off_t length)
{
  soColorProbe (231, "07;31", "soTruncate (\"%s\", %u)\n", ePath, length);
  
  int status;
  uint32_t p_nInodeDir;
  uint32_t p_nInodeEnt;

  // get file inode number
  if((status=soGetDirEntryByPath(ePath,&p_nInodeDir,&p_nInodeEnt))!=0) return status;
  if((status = soAccessGranted (p_nInodeEnt, R)) != 0) return status;
  if((status = soAccessGranted (p_nInodeEnt, W)) != 0) return status;

  SOInode p_inode;
  // get file inode
  if((status=soReadInode (&p_inode,p_nInodeEnt))!=0) return status;

  // check if resize is to truncate or extend
  int diff = p_inode.size - length;

  uint32_t p_clustIndIn;
  uint32_t p_offset;

  uint32_t p_outVal;
  uint32_t i;

  // get cluster index and cluster offset based on the cut point
  soConvertBPIDC(diff-1,&p_clustIndIn,&p_offset);
  
  //if diff>0 is to truncate
  if(diff>0 && p_offset==0){
    if((status=soHandleFileClusters (p_nInodeEnt,p_clustIndIn))!=0) return status;
  }
  else if(diff>0 && p_offset!=0){
    if((status=soHandleFileClusters (p_nInodeEnt,(p_clustIndIn)+1))!=0) return status;
    SODataClust *cluster = malloc(sizeof(SODataClust));
    if((status=soReadFileCluster(p_nInodeEnt,p_clustIndIn,cluster))!=0) return status;
    memset(&cluster->data[p_offset],0,CLUSTER_SIZE-(p_offset));
    if((status=soWriteFileCluster(p_nInodeEnt,p_clustIndIn,cluster))!=0) return status;
  }

  // if diff<0 is to extend
  else if(diff<0 && p_offset==0){
    for(i=p_clustIndIn+1;i<length/BSLPC;i++)
      if((status=soHandleFileCluster(p_nInodeEnt,i,ALLOC,&p_outVal))!=0) return status;
  }
  else if(diff<0 && p_offset!=0){
    for(i=p_clustIndIn+1;i<(length/BSLPC)+1;i++)
      if((status=soHandleFileCluster(p_nInodeEnt,i,ALLOC,&p_outVal))!=0) return status;
  }
  p_inode.size = length;

  if( p_inode.size%2048 == 0){
    p_inode.clucount = p_inode.size/2048;
  }
  else{
    p_inode.clucount = (p_inode.size/2048)+1;
  }
  
  if((status=soWriteInode(&p_inode,p_nInodeEnt))!=0) return status;
  return 0;
}
