/**
 *  \file soFreeInode.c (implementation file)
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

/**
 *  \brief Free the referenced inode.
 *
 *  The inode must be in use, belong to one of the legal file types and have no directory entries associated with it
 *  (refcount = 0). The inode is marked free and inserted in the list of free inodes.
 *
 *  Notice that the inode 0, supposed to belong to the file system root directory, can not be freed.
 *
 *  The only affected fields are:
 *     \li the free flag of mode field, which is set
 *     \li the <em>time of last file modification</em> and <em>time of last file access</em> fields, which change their
 *         meaning: they are replaced by the <em>prev</em> and <em>next</em> pointers in the double-linked list of free
 *         inodes.
 * *
 *  \param nInode number of the inode to be freed
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c ESBTINPINVAL, if the table of inodes metadata in the superblock is inconsistent
 *  \return -\c ETINDLLINVAL, if the double-linked list of free inodes is inconsistent
 *  \return -\c EFININVAL, if a free inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soFreeInode (uint32_t nInode)
{
    soColorProbe (612, "07;31", "soFreeInode (%"PRIu32")\n", nInode);

    int status;                /* status of operation */
    SOSuperBlock* p_sb;        /* super block */
    SOInode* p_inodeblock;     /* pointer to the content of a block of the inode table */
    uint32_t nBlk;             /* index to the location where the logical block number is to be stored for inode */
    uint32_t offset;           /* location in the block where the inode is stored */
    uint32_t ihdtlBlk;         /* index to the location where the logical block number is to be stored for ihdtl */
    uint32_t ihdtlOffset;      /* location in the block where the ihdtl is stored */

    /* loads super block to internal storage */ 
    if((status = soLoadSuperBlock()) != 0) return status;

    /* gets super block from internal storage */
    if((p_sb = soGetSuperBlock()) == NULL) return -EIO;

    /* if the inode is out of range return -EINVAL */
    if(nInode == 0 || nInode >= p_sb->itotal) return -EINVAL;

    /* get inode block and offset */
    if((status = soConvertRefInT(nInode, &nBlk, &offset)) != 0) return status; 

    /* load inode block into internal storage */
    if((status = soLoadBlockInT(nBlk)) != 0) return status;

    /* gets inode's pointer from internal storage */
    if((p_inodeblock = soGetBlockInT()) == NULL ) return -EIO;

    /* check if the block is consistent */
    if((status = soQCheckInodeIU(p_sb, &p_inodeblock[offset])) != 0) return status;

    /* the inode must be an inode in use, with the field refcount to 0*/
    if(p_inodeblock[offset].refcount != 0) return -EIUININVAL;

    p_inodeblock[offset].mode |= INODE_FREE; /* change only a bit of INODE_FREE */   

    /* Quick check of an inode is in use */
    if ((status = soQCheckFInode(&p_inodeblock[offset])) != 0) return status;

    if(p_sb->ihdtl == NULL_INODE)  /* Empty list */
    {
        p_inodeblock[offset].vD1.prev = nInode;
        p_inodeblock[offset].vD2.next = nInode;
        p_sb->ihdtl = nInode;
        if((status = soStoreBlockInT()) != 0) return status;
    }
    else
    { 
        if((status = soStoreBlockInT()) != 0) return status;
        if((status = soConvertRefInT(p_sb->ihdtl, &ihdtlBlk, &ihdtlOffset)) != 0) return status;
        if((status = soLoadBlockInT(ihdtlBlk)) != 0) return status;
        if((p_inodeblock = soGetBlockInT()) == NULL ) return -EIO;  
        if(p_inodeblock[ihdtlOffset].vD1.prev == p_sb->ihdtl) /* list with one element */
        {         
            p_inodeblock[ihdtlOffset].vD1.prev = nInode;
            p_inodeblock[ihdtlOffset].vD2.next = nInode;
            if((status =  soStoreBlockInT()) != 0) return status;

            if((status = soLoadBlockInT(nBlk)) != 0) return status;
            if((p_inodeblock = soGetBlockInT()) == NULL ) return -EIO;
            p_inodeblock[offset].vD1.prev = p_sb->ihdtl;
            p_inodeblock[offset].vD2.next = p_sb->ihdtl;
            if((status = soStoreBlockInT()) != 0) return status;
        }
        else    /* list with 2 or more elements */
        {          
            uint32_t prev;
            if((status = soStoreBlockInT()) != 0) return status;
            if((status = soConvertRefInT(p_sb->ihdtl, &ihdtlBlk, &ihdtlOffset)) != 0) return status;
            if((status = soLoadBlockInT(ihdtlBlk)) != 0) return status;
            if((p_inodeblock = soGetBlockInT()) == NULL ) return -EIO;         
            prev = p_inodeblock[ihdtlOffset].vD1.prev;     
            p_inodeblock[ihdtlOffset].vD1.prev = nInode;
            if((status = soStoreBlockInT()) != 0) return status;

            uint32_t prevBlk, prevOffset;
            if((status = soConvertRefInT(prev, &prevBlk, &prevOffset)) != 0) return status;
            if((status = soLoadBlockInT(prevBlk)) != 0) return status;
            if((p_inodeblock = soGetBlockInT()) == NULL ) return -EIO;
            p_inodeblock[prevOffset].vD2.next = nInode;
            if((status = soStoreBlockInT()) != 0) return status;    

            if((status = soLoadBlockInT(nBlk)) != 0) return status;
            if((p_inodeblock = soGetBlockInT()) == NULL ) return -EIO;
            p_inodeblock[offset].vD1.prev = prev;
            p_inodeblock[offset].vD2.next = p_sb->ihdtl;
            if((status = soStoreBlockInT()) != 0) return status;   
        }
    }

    /* increment number of free inodes */    
    p_sb->ifree += 1;

    /* Quick check of the table of inodes metadata ->  Both the associated fields in the superblock and the table of inodes are checked for consistency. */
     if((status = soQCheckInT(p_sb) != 0)) return status;

    /* Store current block */ 
    if((status = soStoreSuperBlock()) != 0) return status; 

    return 0;
}
