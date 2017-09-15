/**
 *  \file soWriteInode.c (implementation file)
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
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"

/**
 *  \brief Write specific inode data to the table of inodes.
 *
 *  The inode must be in use and belong to one of the legal file types.
 *  Upon writing, the <em>time of last file modification</em> and <em>time of last file access</em> fields are set to
 *  current time.
 *
 *  \param p_inode pointer to the buffer containing the data to be written from
 *  \param nInode number of the inode to be written into
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>buffer pointer</em> is \c NULL or the <em>inode number</em> is out of range
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soWriteInode (SOInode *p_inode, uint32_t nInode)
{
  	soColorProbe (512, "07;31", "soWriteInode (%p, %"PRIu32")\n", p_inode, nInode);

	int stat;					/*verificação erros*/
   	SOSuperBlock* p_sb;			/*superbloco*/
   	uint32_t p_Blk, p_off;		/* n do bloco e offset correspondente ao n do no na tabela*/
   	SOInode *iNodeBlock;			/*bloco de nos onde se situa nInode*/

  	if((stat = soLoadSuperBlock())!=0) return stat; 
   	if((p_sb=soGetSuperBlock())==NULL) return -EIO;
   	if((stat = soQCheckSuperBlock (p_sb))!=0)	return stat;

   	if(nInode < 0 || nInode >= p_sb->itotal) return -EINVAL;			/*verifica n do nó*/

   	if(p_inode == NULL) return -EIUININVAL;								/*verifica ponteiro do nó*/
   	if((stat = soQCheckInodeIU(p_sb,p_inode)) != 0) return stat;	/*verifica consistência do no*/

   	if((stat = soConvertRefInT(nInode,&p_Blk,&p_off)) != 0) return stat; 	/* n do bloco e offset correspondente ao n do no na tabela*/

   	if ((stat = soLoadBlockInT(p_Blk)) != 0) return stat;
   	iNodeBlock = soGetBlockInT();

    /* inode must be associated a valid type */
    if((iNodeBlock->mode & INODE_TYPE_MASK)==0) return -EIUININVAL;

   	iNodeBlock[p_off] = *p_inode;

   	iNodeBlock[p_off].vD1.atime = time(NULL); /*time.h*/
   	iNodeBlock[p_off].vD2.mtime = time(NULL);

   	if ((stat = soStoreBlockInT()) != 0) return stat;
    //if ((stat = soStoreSuperBlock()) != 0) return stat;

  	return 0;
}
