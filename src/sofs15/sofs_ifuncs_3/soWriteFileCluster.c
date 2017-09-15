/**
 *  \file soWriteFileCluster.c (implementation file)
 *
 *  \author Tiago Bastos
 */

#include <stdio.h>
#include <inttypes.h>
#include <errno.h>

#include "sofs_probe.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_datacluster.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_1.h"
#include "sofs_ifuncs_2.h"

/** \brief operation get the logical number of the referenced data cluster */
#define GET         0
/** \brief operation  allocate a new data cluster and include it into the list of references of the inode which
 *                    describes the file */
#define ALLOC       1
/** \brief operation free the referenced data cluster and dissociate it from the inode which describes the file */
#define FREE        2

/* Allusion to external function */

int soHandleFileCluster (uint32_t nInode, uint32_t clustInd, uint32_t op, uint32_t *p_outVal);
extern void *memcpy(void *dest, void *src, size_t count);
/**
 *  \brief Write a specific data cluster.
 *
 *  Data is written into a specific data cluster which is supposed to belong to an inode associated to a file (a regular
 *  file, a directory or a symbolic link). Thus, the inode must be in use and belong to one of the legal file types.
 *
 *  If the referred cluster has not been allocated yet, it will be allocated now so that the data can be stored as its
 *  contents.
 *
 *  \param nInode number of the inode associated to the file
 *  \param clustInd index to the list of direct references belonging to the inode where the reference to the data cluster
 *                  whose contents is to be written is stored
 *  \param buff pointer to the buffer where data must be written from
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> or the <em>index to the list of direct references</em> are out of
 *                      range or the <em>pointer to the buffer area</em> is \c NULL
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soWriteFileCluster (uint32_t nInode, uint32_t clustInd, void *buff)
{
  soColorProbe (412, "07;31", "soWriteFileCluster (%"PRIu32", %"PRIu32", %p)\n", nInode, clustInd, buff);

  SOSuperBlock *p_sb;     /*pointer to superblock*/
  SODataClust datcl;      /*data cluster*/
  SOInode inode;        /*inode*/
  uint32_t nClust;      /*cluster number*/
  int stat;         /*status of operation*/
  int i;            /*for loop*/
  
  /*load superblock*/
  if ((stat = soLoadSuperBlock()) != 0)
    return stat;
    
  /*get superblock*/
  p_sb = soGetSuperBlock();
  
  /*check value of inode */
  if (nInode<0 || nInode>=p_sb->itotal)
    return -EINVAL;
    
  /*check value of cluster*/
  if (clustInd>=(N_DIRECT+RPC*(RPC+1)))
    return -EINVAL;
    
  /*buff can't be NULL*/
  if (buff == NULL)
    return -EINVAL;
    
  /*get cluster number*/  
  if ((stat = soHandleFileCluster(nInode,clustInd,GET,&nClust)) != 0)
    return stat;
  
  /*cluster processing*/  
  if (nClust == NULL_CLUSTER)
    if ((stat = soHandleFileCluster(nInode,clustInd,ALLOC,&nClust)) != 0)
      return stat;
        
  for (i=0 ; i<BSLPC ; i++)
    datcl.data[i] = ((char*)buff)[i];
      
  if ((stat = soWriteCacheCluster(p_sb->dzone_start+(nClust*BLOCKS_PER_CLUSTER),&datcl)) != 0)
    return stat;
  
  /*Update times*/  
  if ((stat = soReadInode(&inode,nInode)) != 0)
    return stat;

  if ((stat = soWriteInode(&inode,nInode)) != 0)
    return stat;

  return 0;
}