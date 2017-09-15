/**
 *  \file soFreeDataCluster.c (implementation file)
 *
 *  \author António Rui Borges - September 2012
 */

#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include "sofs_probe.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_datacluster.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"

/* Allusion to internal functions */

int soDeplete (SOSuperBlock *p_sb);

/**
 *  \brief Free the referenced data cluster.
 *
 *  The cluster is inserted into the insertion cache of free data cluster references. If the cache is full, it has to be
 *  depleted before the insertion may take place. It has to have been previouly allocated.
 *
 *  Notice that the first data cluster, supposed to belong to the file system root directory, can never be freed.
 *
 *  \param nClust logical number of the data cluster
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, the <em>data cluster number</em> is out of range
 *  \return -\c EDCNALINVAL, if the data cluster has not been previously allocated
 *  \return -\c ESBDZINVAL, if the data zone metadata in the superblock is inconsistent
 *  \return -\c ESBFCCINVAL, if the free data clusters caches in the superblock are inconsistent
 *  \return -\c EFCTINVAL, if the table of references to free data clusters is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soFreeDataCluster (uint32_t nClust)
{
   soColorProbe (614, "07;33", "soFreeDataCluster (%"PRIu32")\n", nClust);

   int status;
   SOSuperBlock* p_sb;
   uint32_t p_data;

   /* Start validating arguments */
   if((status=soLoadSuperBlock())!=0) return status; // Checking previous erros on loading/storing the superblock  
   if((p_sb=soGetSuperBlock())==NULL) return -EIO;   // NULL, if there is an error on a previous load/store operation 
   if(nClust<1 || nClust>p_sb->dzone_total-1) return -EINVAL; // Check if cluster logical number is a valid one 

   if((status=soQCheckSuperBlock(p_sb))!=0) return status;            // Quick check of the superblock metadata
   if((status=soQCheckStatDC(p_sb,nClust,&p_data))!=0) return status; // Quick check of the allocation status of a data cluster
   if(p_data!=ALLOC_CLT) return -EDCNALINVAL; // Check if data cluster is marked as allocated

   /* Start of the algorithm */ 
   if(p_sb->dzone_insert.cache_idx == DZONE_CACHE_SIZE) // Insertion cache is full
      soDeplete(p_sb);
   p_sb->dzone_insert.cache[p_sb->dzone_insert.cache_idx] = nClust;
   p_sb->dzone_insert.cache_idx += 1;
   p_sb->dzone_free += 1;

   if((status=soStoreSuperBlock())!=0) return status; // Check previous/current error on loading/storing a single indirect references cluster
   return 0;
}

/**
 *  \brief Deplete the insertion cache of references to free data clusters.
 *
 *  \param p_sb pointer to a buffer where the superblock data is stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soDeplete (SOSuperBlock *p_sb)
{
  	uint32_t index;								// número da referência
  	uint32_t n;									// ciclo for
  	uint32_t nblock;							// numero do bloco
  	uint32_t offset;							// offset
  	uint32_t *ref;								// ponteiro para o bloco de referencias
  	uint32_t status;							// variavel de erro

  	index = p_sb->tbfreeclust_tail;				// index do array de elementos da tabela de referências de free data clusters

  	for (n = 0; n < p_sb->dzone_insert.cache_idx; n++)
    {
    	//  conversão de um índice da tabela no número [lógico] do bloco e no deslocamento [dentro do bloco] 
    	// onde a referencia associada está localizada
		if((status = soConvertRefFCT(index,&nblock,&offset)) != 0)
			return status;
		// carrega o conteúdo de um bloco especifico da tabela de referencias de free data clusters no armazentamento interno
		if((status = soLoadBlockFCT(nblock)) != 0)
			return status;
		// retorna um ponteiro para a área de armazenamento interno: 
		if((ref = soGetBlockFCT()) == NULL)
			return -EIO;

		// adiciona a ref do free cluster da cache á tabela de referencias
		ref[index] = p_sb->dzone_insert.cache[n];
		// coloca o slot de cache a NULL CLUSTER
		p_sb->dzone_insert.cache[n] = NULL_CLUSTER;
		// incrementa e calcula o index
		index = (index + 1) % p_sb->dzone_total;

		// guarda o conteúdo do bloco da tabela de referencias de free data clusters residente no armazenamento interno
		if((status = soStoreBlockFCT()) != 0) 
			return status;
	}    	

	// restart pointer to empty cache
	p_sb->dzone_insert.cache_idx = 0;
	// coloca a cauda das ref's para free data clusters numa nova posiçao
	p_sb->tbfreeclust_tail = index;

	return 0;
}
