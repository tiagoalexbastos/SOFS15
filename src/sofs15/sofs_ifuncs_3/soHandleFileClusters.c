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
void * malloc ( size_t size );
int dIndirectas(SOInode* p_Inode, uint32_t* i, SOSuperBlock* p_sb, uint32_t nInode);
int sIndirectas(SOInode* p_Inode, uint32_t* i, SOSuperBlock* p_sb, uint32_t nInode);
int directas(SOInode* p_Inode, uint32_t* i, uint32_t nInode);

/**
 *  \brief Handle all data clusters from the list of references starting at a given point.
 *
 *  The file (a regular file, a directory or a symlink) is described by the inode it is associated to.
 *
 *  Only one operation (FREE) is available and can be applied to the file data clusters starting from the index to the
 *  list of direct references which is given.
 *
 *  The field <em>clucount</em> and the lists of direct references, single indirect references and double indirect
 *  references to data clusters of the inode associated to the file are updated.
 *
 *  Thus, the inode must be in use and belong to one of the legal file types.
 *
 *  \param nInode number of the inode associated to the file
 *  \param clustIndIn index to the list of direct references belonging to the inode which is referred (it contains the
 *                    index of the first data cluster to be processed)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> or the <em>index to the list of direct references</em> are out of
 *                      range
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soHandleFileClusters (uint32_t nInode, uint32_t clustIndIn){
  soColorProbe (414, "07;31", "soHandleFileClusters (%"PRIu32", %"PRIu32")\n", nInode, clustIndIn);


  int err;                      
  uint32_t i;                   
  SOInode *p_Inode;  // inode
  SOSuperBlock *p_sb;           

  // load and check supeblock's consistency
  if((err=soLoadSuperBlock())!=0) return err; 
  if((p_sb=soGetSuperBlock())== NULL) return -EIO;
  if((err=soQCheckSuperBlock(p_sb))!=0) return err;
  if((err=soQCheckInT(p_sb))!= 0) return err;
  
  // store inode in memory
  if( (p_Inode = (SOInode*) malloc(sizeof(SOInode))) == NULL ) return -ELIBBAD;
  if((err=soReadInode(p_Inode, nInode)) != 0) return err;
  
  i = clustIndIn; // start with the frst index       
  
  while(i < MAX_FILE_CLUSTERS){
    
    // check if it is a double indirect reference
    if(i>=(N_DIRECT + RPC) && i < (N_DIRECT+RPC*(1+RPC))){     
      if((err=dIndirectas(p_Inode, &i, p_sb, nInode)) != 0) return err;   
    }
   
    // check if it is a single indirect reference
    else if((i>N_DIRECT) && (i<(N_DIRECT + RPC))){    

      // check if there are double indirect references. If so, free them at first                                    
      if (p_Inode->i2 != NULL_CLUSTER){                                
        uint32_t j = N_DIRECT+RPC;
        while(j < MAX_FILE_CLUSTERS)
          if((err=dIndirectas(p_Inode, &j, p_sb, nInode)) != 0) return err;
      }  
      // free single indirect clusters                                                                   
      if((err=sIndirectas( p_Inode, &i, p_sb, nInode)) != 0) return err; 
    }  

    // check if it is a direct reference                                                                         
    else {
      // check if there are double indirect references. If so, free double indirect clusters                                         
      if (p_Inode->i2 != NULL_CLUSTER)        {                               
        uint32_t j = N_DIRECT+RPC;
        while(j < MAX_FILE_CLUSTERS)
                 if((err=dIndirectas(p_Inode, &j, p_sb, nInode)) != 0) return err;
      }
      
      // check if there are single indirect references. If so, free single indirect clusters                            
      if (p_Inode->i1 != NULL_CLUSTER){                                
        uint32_t j = N_DIRECT;
        while(j < N_DIRECT+RPC){       
          if((err=sIndirectas(p_Inode, &j, p_sb, nInode)) != 0) return err;
        }
      }   
      // free direct clusters                                                     
      if((err=directas(p_Inode, &i, nInode)) != 0) return err; 
    }

  }
  return 0;
}

// free double indirect clusters
int directas(SOInode* p_Inode, uint32_t* i, uint32_t nInode){
  int err;   
  // if the reference point to a cluster, free it    
  if (p_Inode->d[*i] != NULL_CLUSTER) 
    if((err = soHandleFileCluster(nInode, *i, FREE, NULL)) != 0) return err;
          
  (*i)++; 
  return 0;
}

// free single indirect clusters
int sIndirectas(SOInode* p_Inode, uint32_t* i, SOSuperBlock* p_sb, uint32_t nInode){
  SODataClust *p_refd;  // d' 
  int err;              
  
  // if i1 points to a cluster, load d'
  if(p_Inode->i1 != NULL_CLUSTER){         
    
    if((err = soLoadDirRefClust((p_Inode->i1)*BLOCKS_PER_CLUSTER+(p_sb->dzone_start))) != 0) return err;     
    if((p_refd = soGetDirRefClust())== NULL) return -EIO;         

    // if the reference point to a cluster, free it           
    if (p_refd->ref[*i-N_DIRECT] != NULL_CLUSTER) 
      if((err = soHandleFileCluster(nInode, *i, FREE, NULL)) != 0) return err;
    
    (*i)++; 
  }
  else {     
    *i += RPC;
  }
  return 0;
}

// free direct clusters
int dIndirectas(SOInode* p_Inode, uint32_t* i, SOSuperBlock* p_sb, uint32_t nInode){
  SODataClust *p_refi; // i'
  SODataClust *p_refd; // d''
  uint32_t cluster;    // i' index
  uint32_t indice_i2;  
  uint32_t offset;     // d'' index
  uint32_t j;          
  int err;             

  // if i2 points to a cluster, load i'
  if (p_Inode->i2 != NULL_CLUSTER)  {                              
    if((err = soLoadSngIndRefClust((p_Inode->i2)*BLOCKS_PER_CLUSTER+(p_sb->dzone_start)))!= 0) return err;                   
    if((p_refi = soGetSngIndRefClust())== NULL) return -EIO;
                   
    indice_i2 = *i - N_DIRECT - RPC;
    cluster = indice_i2 / RPC;
    offset = indice_i2 % RPC;

    // if ref[(l-N_DIRECT-RPC)/RPC] points to a cluster, load d''
    if (p_refi->ref[cluster] != NULL_CLUSTER) {                                         
            if ((err = soLoadDirRefClust((p_refi->ref[cluster])*BLOCKS_PER_CLUSTER+(p_sb->dzone_start))) != 0) return err;      
            if((p_refd = soGetDirRefClust())== NULL) return -EIO;
            
            // free all d'' clusters, starting on offset
            for(j=offset; j < RPC; j++) {                           
                    if (p_refd->ref[j] != NULL_CLUSTER)
                            if((err = soHandleFileCluster(nInode, *i, FREE, NULL)) != 0) return err;
                    (*i)++; 
            }
    }
    // if i' == NULL_CLUSTER, move to next cluster
    else {     
      *i += RPC;
    }
  }
  else 
  {    
    *i += (RPC * RPC);
  }
  return 0;
}
