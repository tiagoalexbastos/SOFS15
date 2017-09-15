/**
 *  \file soAllocInode.c (implementation file)
 *
 *  \author João Rodrigues
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

/**
 *  \brief Allocate a free inode.
 *
 *  The inode is retrieved from the list of free inodes, marked in use, associated to the legal file type passed as
 *  a parameter and generally initialized. It must be free.
 *
 *  Upon initialization, the new inode has:
 *     \li the field mode set to the given type, while the free flag and the permissions are reset
 *     \li the owner and group fields set to current userid and groupid
 *     \li the <em>prev</em> and <em>next</em> fields, pointers in the double-linked list of free inodes, change their
 *         meaning: they are replaced by the <em>time of last file modification</em> and <em>time of last file
 *         access</em> which are set to current time
 *     \li the reference fields set to NULL_CLUSTER
 *     \li all other fields reset.

 *  \param type the inode type (it must represent either a file, or a directory, or a symbolic link)
 *  \param p_nInode pointer to the location where the number of the just allocated inode is to be stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>type</em> is illegal or the <em>pointer to inode number</em> is \c NULL
 *  \return -\c ENOSPC, if the list of free inodes is empty
 *  \return -\c ESBTINPINVAL, if the table of inodes metadata in the superblock is inconsistent
 *  \return -\c ETINDLLINVAL, if the double-linked list of free inodes is inconsistent
 *  \return -\c EFININVAL, if a free inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soAllocInode (uint32_t type, uint32_t* p_nInode)
{
	soColorProbe (611, "07;31", "soAllocInode (%"PRIu32", %p)\n", type, p_nInode);

	if ((((type & 0x0800) != (INODE_DIR & 0x0800))) && (((type & 0x0400) != (INODE_FILE & 0x0400))) && (((type & 0x0200) != (INODE_SYMLINK & 0x0200))))
		return -EINVAL;	/*ERRO: tipo invalido*/

	if(p_nInode == NULL){
    return -EINVAL;
  }

	int stat;	/*usado para testes*/
	SOSuperBlock *sb;

	if((stat = soLoadSuperBlock()) != 0) return stat;

	if ((sb = soGetSuperBlock ()) == NULL)	/*super bloco*/ 
		return -EIO;

	if((stat = soQCheckSuperBlock (sb))!=0)	/*consistencia super bloco*/
		return stat;
	if ((stat = soQCheckInT (sb)) != 0)		/*consistencia tabela nos-i */
		return stat;   		
		
	if (sb->ifree == 0)
		return -ENOSPC;					/*ERRO: nao existem nos livres*/

	uint32_t ihdtl = sb->ihdtl;						/*indice da cabeca*/
	uint32_t ninode = ihdtl;						/*índice nó livre*/

	uint32_t p_nBlk, p_off;								/*n do bloco e offset da cabeca*/


	if ((stat = soConvertRefInT(ihdtl , &p_nBlk , &p_off)) != 0)	/*bloco e offset da cabeca da lista*/
		return stat;
	if ((stat = soLoadBlockInT(p_nBlk)) != 0)
		return stat;

	SOInode *inode = soGetBlockInT();				/*estrutura onde esta a cabeca*/

	if ((stat = soQCheckFInode(&inode[p_off])) != 0)
	  return stat;

	if (inode[p_off].vD1.prev == inode[p_off].vD2.next)
	{ /* a lista tem um ou dois elementos */
		if (inode[p_off].vD1.prev == ihdtl){
         if ((stat = soStoreBlockInT()) != 0)
            return stat;

		   /* a lista tem um elemento */
			ihdtl = NULL_INODE;
      }
		else { /* a lista tem dois elementos */
			uint32_t next = inode[p_off].vD2.next;
			uint32_t p_nBlkNext, p_offNext;				/*bloco e offset do no next*/

			if ((stat = soConvertRefInT(next , &p_nBlkNext , &p_offNext)) != 0)	/*bloco e offset do no next*/
   				return stat;

			if(p_nBlkNext != p_nBlk){ 
				/*next e cabeca estao em nos diferentes*/
				if ((stat = soStoreBlockInT()) != 0)
					return stat;
				if ((stat = soLoadBlockInT(p_nBlkNext)) != 0)						/*bloco onde o next esta situado*/
					return stat;
				inode = soGetBlockInT();										/*estrutura onde esta next*/
			}

			inode[p_offNext].vD1.prev = inode[p_offNext].vD2.next = next;

			if ((stat = soStoreBlockInT()) != 0)
   				return stat;

   		ihdtl = next;
		}
	}
	else { /* a lista tem mais de dois elementos */
		uint32_t next = inode[p_off].vD2.next;		/*indice do no next*/
		uint32_t prev = inode[p_off].vD1.prev;		/*indice do no prev*/

		uint32_t p_nBlkNext, p_offNext;				/*bloco e offset do no next*/
		uint32_t p_nBlkPrev, p_offPrev;				/*bloco e offset do no prev*/

		if ((stat = soConvertRefInT(next , &p_nBlkNext , &p_offNext)) != 0)	/*bloco e offset do next*/
   		return stat;
		if ((stat = soConvertRefInT(prev , &p_nBlkPrev , &p_offPrev)) != 0)	/*bloco e offset do prev*/
			return stat;

		if( p_nBlkNext != p_nBlk )									
	   {
		/*next e cabeca estao em nos diferentes*/
			if ((stat = soStoreBlockInT()) != 0) 						/* guardar bloco*/
   			return stat;

			/*carregar bloco onde esta next*/
			if ((stat = soLoadBlockInT(p_nBlkNext)) != 0)						/*carregar bloco onde o next esta situado*/
				return stat;
			inode = soGetBlockInT();										/*estrutura onde esta next*/
	    }	

		inode[p_offNext].vD1.prev = prev;

		/*	se next e prev nao estiverem no mesmo bloco, guardar bloco onde esta next e carregar bloco
		  	onde esta prev*/
		if( p_nBlkNext != p_nBlkPrev )									
		{
			if ((stat = soStoreBlockInT()) != 0) 						/* guardar no*/
   				return stat;

   			/*carregar bloco onde esta prev*/
   			if ((stat = soLoadBlockInT(p_nBlkPrev)) != 0)
   				return stat;
   			inode = soGetBlockInT();
		}

		inode[p_offPrev].vD2.next = next;
		ihdtl = next;

		if ((stat = soStoreBlockInT()) != 0) 						/* guardar no*/
   			return stat;
	}

	if ((stat = soLoadBlockInT(p_nBlk)) != 0)
		return stat;
	inode = soGetBlockInT();


  inode[p_off].mode = (0x0E00 & type);                                      // tipo
	inode[p_off].refcount = 0;                                                // referencias
	inode[p_off].owner = (uint32_t) getuid();                         		  // owner
	inode[p_off].group = (uint32_t) getgid();                         		  // grupo
	inode[p_off].size = 0;                                                    // tamanho
	inode[p_off].clucount = 0;                                                // n ce clusters
	inode[p_off].vD1.atime = (uint32_t) time(NULL);                           // tempo de acesso
	inode[p_off].vD2.mtime = (uint32_t) time(NULL);                           // tempo de modificacao
	uint32_t nDC;
	for(nDC = 0; nDC < N_DIRECT; nDC++){

	  inode[p_off].d[nDC] = NULL_CLUSTER;                                     // tabela de referencias diretas
	}
	inode[p_off].i1 = NULL_CLUSTER;                                           // tabela de referencias simplesmente indiretas
	inode[p_off].i2 = NULL_CLUSTER;                                           // tabela de referencias duplamente indiretas

	if ((stat = soStoreBlockInT()) != 0) 						/* guardar bloco*/
   			return stat;

	/*atualizar super bloco*/
	sb->ifree -= 1;
	sb->ihdtl = ihdtl;

	soStoreSuperBlock();	/*gravar superbloco*/

	*p_nInode = ninode;		/*dar indice nó livre*/

   return 0;
}
