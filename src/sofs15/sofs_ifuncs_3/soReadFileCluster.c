/**
 *  \file soReadFileCluster.c (implementation file)
 *
 *  \author João Rodrigues
 */

#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>		//biblioteca com memcpy() e memset()

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

/**
 *  \brief Read a specific data cluster.
 *
 *  Data is read from a specific data cluster which is supposed to belong to an inode associated to a file (a regular
 *  file, a directory or a symbolic link). Thus, the inode must be in use and belong to one of the legal file types.
 *
 *  If the referred cluster has not been allocated yet, the returned data will consist of a byte stream filled with the
 *  character null (ascii code 0).
 *
 *  \param nInode number of the inode associated to the file
 *  \param clustInd index to the list of direct references belonging to the inode where the reference to the data cluster
 *                  whose contents is to be read is stored
 *  \param buff pointer to the buffer where data must be read into
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

int soReadFileCluster (uint32_t nInode, uint32_t clustInd, void *buff)
{
	soColorProbe (411, "07;31", "soReadFileCluster (%"PRIu32", %"PRIu32", %p)\n", nInode, clustInd, buff);

	/*int stat;								//usado em testes

	//carregar superbloco
	if((stat = soLoadSuperBlock()) != 0)	return stat;
	SOSuperBlock *p_sb;
	if((p_sb = soGetSuperBlock()) == NULL) return -EIUININVAL;

	uint32_t logicClust;


	if ((stat = soHandleFileCluster (nInode, clustInd, GET, &logicClust)) != 0) return stat;	//buscar n logico do cluster
	if(logicClust == NULL_CLUSTER) {memset(buff , '\0', CLUSTER_SIZE); return 0;}				
	if ((stat = soLoadDirRefClust(p_sb->dzone_start + logicClust * BLOCKS_PER_CLUSTER)) != 0) return stat;
	SODataClust *data = soGetDirRefClust();	
	memcpy(buff, data->data, CLUSTER_SIZE);

	return 0;


}*/


int stat;								//usado em testes

	//carregar superbloco
	if((stat = soLoadSuperBlock()) != 0)	return stat;
	SOSuperBlock *p_sb;
	if((p_sb = soGetSuperBlock()) == NULL) return -EIUININVAL;

	uint32_t logicClust;


	if ((stat = soHandleFileCluster (nInode, clustInd, GET, &logicClust)) != 0) return stat;	//buscar n logico do cluster
	if(logicClust == NULL_CLUSTER) {memset(buff , '\0', CLUSTER_SIZE); return 0;}	
	/*			
	if ((stat = soLoadDirRefClust(p_sb->dzone_start + logicClust * BLOCKS_PER_CLUSTER)) != 0) return stat;
	SODataClust *data = soGetDirRefClust();	*/

	SODataClust data;
	if((stat = soReadCacheCluster(p_sb->dzone_start + logicClust * BLOCKS_PER_CLUSTER, &data)) != 0)	// reads data cluster from buffercache to the data cluster created
      return stat;
	memcpy(buff, data.data, CLUSTER_SIZE);

	return 0;


}
