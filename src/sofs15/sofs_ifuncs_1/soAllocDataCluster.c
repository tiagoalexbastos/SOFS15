/**
 *  \file soAllocDataClustere.c (implementation file)
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

int soReplenish (SOSuperBlock *p_sb);
int soDeplete (SOSuperBlock *p_sb);

/**
 *  \brief Allocate a free data cluster.
 *
 *  The cluster is retrieved from the retrieval cache of free data cluster references. If the cache is empty, it has to
 *  be replenished before the retrieval may take place.
 *
 *  \param p_nClust pointer to the location where the logical number of the allocated data cluster is to be stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, the <em>pointer to the logical data cluster number</em> is \c NULL
 *  \return -\c ENOSPC, if there are no free data clusters
 *  \return -\c ESBDZINVAL, if the data zone metadata in the superblock is inconsistent
 *  \return -\c ESBFCCINVAL, if the free data clusters caches in the superblock are inconsistent
 *  \return -\c EFCTINVAL, if the table of references to free data clusters is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */


int soAllocDataCluster (uint32_t *p_nClust)
{
   soColorProbe (613, "07;33", "soAllocDataCluster (%p)\n", p_nClust); 
 
/*-------------------------------------------*/
/*              comformidade                 */
/*-------------------------------------------*/
    /*the pointer to the logical data is NULL*/
    if(p_nClust == NULL) return -EINVAL;
 
    SOSuperBlock* pointSuperB;                             /* pointer to superblock */
     
    int status;                              /* error value */  
     
    // Load Super Block
    if((status = soLoadSuperBlock()) != 0)      /*Transfer the contents of the superblock*/
      return status;
    
    // Get Super Block
    pointSuperB = soGetSuperBlock();       /*pointer to superblock*/ 


    if((status=soQCheckDZ(pointSuperB))!= 0 )
        return status;                      // if the data zone metadata in the superblock is inconsistent

	if((status=soQCheckSuperBlock(pointSuperB))!= 0) //consistency
        return status; 

    if(pointSuperB->dzone_free == 0 ) 
        return -ENOSPC;              /*if there are no free data clusters*/

   
/*-------------------------------------------*/
/*              algoritmo 1                  */
/*-------------------------------------------*/

    if (pointSuperB->dzone_retriev.cache_idx == DZONE_CACHE_SIZE) { 
        /* cache empty,replenish */
        if ((status = soReplenish(pointSuperB))!=0)
            return status;
    }

    /*p_nClust pointer to the location where the logical number of the allocated data cluster is to be stored*/
    *p_nClust = pointSuperB->dzone_retriev.cache[pointSuperB->dzone_retriev.cache_idx];
    pointSuperB->dzone_retriev.cache_idx += 1;                            /*increment cahe index*/
    pointSuperB->dzone_free -= 1;                                  /*decrement free zone */

/*-------------------------------------------*/


    if ((status = soStoreSuperBlock()) != 0)
        return status;                          /*store sb*/
    return 0;
}


/**
 *  \brief Replenish the retrieval cache of references to free data clusters.
 *
 *  \param p_sb pointer to a buffer where the superblock data is stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soReplenish (SOSuperBlock *p_sb)
{
	
	uint32_t nclustt = (p_sb->dzone_free < DZONE_CACHE_SIZE) ? p_sb->dzone_free : DZONE_CACHE_SIZE;
	uint32_t index = p_sb->tbfreeclust_head; //valor inicial
	uint32_t n;
	uint32_t *ref;  	// Array que aponta para a região de referências do data cluster. Não pode ser NULL.
	uint32_t error;
	uint32_t block;
	uint32_t offset;
	
	/*
	 * 
	 * Ideia do prof: 
	 * carregar o 1º bloco 
	 * fixar o 1º bloco index
	 * pegar no index -> ofsset e bloco
	 * se bloco anterior ao lido prosseguir 
	 * se o bloco mudou - store bloco anterior, load novo e get
	*/
	
	for (n = DZONE_CACHE_SIZE - nclustt; n < DZONE_CACHE_SIZE; n++){
		if(index == p_sb->tbfreeclust_tail) break; //primeira validação : Precisa de ter, pelo menos, um cluster livre
		
		/*Get  references to Block and off_set from the index*/
		if ((error = soConvertRefFCT(index,&block,&offset)) != 0) return error;	
			
		/*Load Block*/
		if ((error = soLoadBlockFCT(block)) != 0) return error;
			
		/*Get pointer to ref from the table of references to free data clusters*/
		if ((ref = soGetBlockFCT()) == NULL) return -EIO;
                   
		p_sb->dzone_retriev.cache[n] = ref[offset];	     /*novas referências*/
		ref[offset]=NULL_CLUSTER;
		index = (index + 1) % p_sb->dzone_total;	     /*incrementar o index*/
		if ((error= soStoreBlockFCT()) != 0) return error;	
	}


	if (n != DZONE_CACHE_SIZE){ 
		if((error=soDeplete(p_sb))!=0)return error;		     /* cache cheia */
		
		for (; n < DZONE_CACHE_SIZE; n++) { 
			/*Get  references to Block and off_set from the index*/
			if ((error = soConvertRefFCT(index,&block,&offset)) != 0) return error;	
			
			/*Load Block*/
			if ((error = soLoadBlockFCT(block)) != 0) return error;
			
			/*Get pointer to ref from the table of references to free data clusters*/
			if ((ref = soGetBlockFCT()) == NULL) return -EIO;

			p_sb->dzone_retriev.cache[n] = ref[offset];        /*novas referências*/
			ref[offset]=NULL_CLUSTER;
			index = (index + 1) % p_sb->dzone_total;           /*incrementar o index*/
		  
		 
			if ((error= soStoreBlockFCT()) != 0) return error;
		}
	}

      p_sb->dzone_retriev.cache_idx = DZONE_CACHE_SIZE - nclustt;   

      p_sb->tbfreeclust_head = index;			          

	
	
	
	
	
	
	
	
	
	
	
	
	/* Código antigo
	 * 
	 * 
	 * 
	
	for(n = DZONE_CACHE_SIZE - nclustt; n < DZONE_CACHE_SIZE; n++){
		if(index == p_sb->tbfreeclust_tail) break;	// First Validation: At least, it needs to have one free cluster!
		

		
		if( (error = soConvertRefFCT(index, &block, &offset)) != 0) return error;	
		
		
		
		if(	(error = soLoadBlockFCT(block)) != 0 ) return error;


		
		if( (ref = soGetBlockFCT()) == NULL ) return -EIO;
	
		p_sb->dzone_retriev.cache[n] = ref[index];   //erro aqui     
		//index global - indice + bloco           
		//limpar o que esta no anel para NUll Cluster
		ref[index]=NULL_CLUSTER;	//done
		index = (index + 1) % p_sb->dzone_total;

		if( (error = soStoreBlockFCT()) != 0 ) return error;
	}
	if(n != DZONE_CACHE_SIZE){ 		// esvaziar a cache de inserção para se obter as referências restantes
		if((error=soDeplete(p_sb))!=0) return error;           
		for(; n < DZONE_CACHE_SIZE; n++){
				
			if( (error = soConvertRefFCT(index, &block, &offset)) != 0) return error;	
			if(	(error = soLoadBlockFCT(block)) != 0 ) return error;
			if( (ref = soGetBlockFCT()) == NULL ) return -EIO;
			

			p_sb->dzone_retriev.cache[n] = ref[index];
			index = (index + 1) % p_sb->dzone_total;
			if( (error = soStoreBlockFCT()) != 0 ) return error;
		}
	}
	p_sb->dzone_retriev.cache_idx = DZONE_CACHE_SIZE - nclustt;
	p_sb->tbfreeclust_head = index;
	*/
	
	return 0;
}
