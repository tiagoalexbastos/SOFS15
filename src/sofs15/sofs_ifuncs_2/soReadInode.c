/**
 *  \file soReadInode.c (implementation file)
 *
 *  \author Joana Conde nºmec.: 35818
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
 *  \brief Read specific inode data from the table of inodes.
 *
 *  The inode must be in use and belong to one of the legal file types.
 *  Upon reading, the <em>time of last file access</em> field is set to current time.
 *
 *  \param p_inode pointer to the buffer where inode data must be read into
 *  \param nInode number of the inode to be read from
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

/*extern void *memcpy(void *dest, void *src, size_t count); */

int soReadInode (SOInode *p_inode, uint32_t nInode)
{
	soColorProbe (511, "07;31", "soReadInode (%p, %"PRIu32")\n", p_inode, nInode);

	/* validate arguments */
	if(p_inode == NULL) return -EINVAL;

	int status;				  /* status of operation */
	SOSuperBlock* p_sb;       /* pointer to superblock */
	uint32_t nBlk;            /* index to the location where the logical block number is to be stored for inode */
    uint32_t offset;          /* location in the block where the inode is stored */
    SOInode *p_inoderead;	  /* pointer to the content of a block of the inode table */

    if (p_inode == NULL)           // Verificar se o ponteiro para a estrutura soInode não é nulo
        return -EINVAL; 
   
	/* loads superblock to internal storage */ 
	if((status = soLoadSuperBlock()) != 0) return status;

	/* gets superblock from internal storage */
	if((p_sb = soGetSuperBlock()) == NULL) return -EIO;

    /* check consistent of superblock */
    if((status = soQCheckSuperBlock(p_sb))!=0) return status;

    if((status = soQCheckInT(p_sb)) != 0)           // Verificar consistência da tabela de nós-i
        return status;
    

	/* if the inode is out of range return -EINVAL */
    if(nInode < 0 || nInode >= p_sb->itotal) return -EINVAL;

    /* get inode block and offset */
    if((status = soConvertRefInT(nInode, &nBlk, &offset)) != 0) return status; 

    /* load inode block into internal storage */
    if((status = soLoadBlockInT(nBlk)) != 0) return status;

    /* gets inode's pointer from internal storage */
    if((p_inoderead = soGetBlockInT()) == NULL ) return -EIO;  

    /* check if the block is consistent */
    /* inode must be in use*/
    if((status = soQCheckInodeIU(p_sb, &p_inoderead[offset])) != 0) return status;  

    /* inode must be associated a valid type */
    if((p_inoderead[offset].mode & INODE_TYPE_MASK)==0 || (p_inoderead[offset].mode & INODE_FREE)) return -EIUININVAL;  

    /* update the last access time */
    p_inoderead[offset].vD1.atime = time(NULL);   

    /* copy the contents of the Inode to the desired region */
    //memcpy(p_inode,p_inoderead,sizeof(SOInode));
    /* or */
    *p_inode = p_inoderead[offset];         //*(p_inoderead + offset);      

    /* Store changes made to the block of the inode p_inode */
    if((status = soStoreBlockInT()) != 0) return status;

    if((status = soStoreSuperBlock()) != 0) return status; 

 	return 0;
}
