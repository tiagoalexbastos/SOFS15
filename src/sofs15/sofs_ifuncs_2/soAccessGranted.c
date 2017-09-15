/**
 *  \file soAccessGranted.c (implementation file)
 *
 *  \author Daniela Sousa, 70158 
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

/* Allusion to internal functions */

int soReadInode (SOInode *p_inode, uint32_t nInode);

/** \brief performing a read operation */
#define R  0x0004
/** \brief performing a write operation */
#define W  0x0002
/** \brief performing an execute operation */
#define X  0x0001

/**
 *  \brief Check the inode access rights against a given operation.
 *
 *  The inode must to be in use and belong to one of the legal file types.
 *  It checks if the inode mask permissions allow a given operation to be performed.
 *
 *  When the calling process is <em>root</em>, access to reading and/or writing is always allowed and access to
 *  execution is allowed provided that either <em>user</em>, <em>group</em> or <em>other</em> have got execution
 *  permission.
 *
 *  \param nInode number of the inode
 *  \param opRequested operation to be performed:
 *                    a bitwise combination of R, W, and X
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range or no operation of the defined class is described
 *  \return -\c EACCES, if the operation is denied
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soAccessGranted (uint32_t nInode, uint32_t opRequested)
{
  soColorProbe (513, "07;31", "soAccessGranted (%"PRIu32", %"PRIu32")\n", nInode, opRequested);


	SOSuperBlock* p_sb;            
    SOInode inode;          

  uint32_t status; /* status control */
  
  /* Verify if request operation is supported*/

  uint32_t u = W | R | X ;
  uint32_t a = W & R & X ;
  if (opRequested > u || opRequested == a) return -EINVAL; 
     
     
    if((status=soLoadSuperBlock())!=0) 
    	return status;
    // get super block pointer
    if((p_sb=soGetSuperBlock())== NULL) 
    	return -EIO;  

  
    if((status=soQCheckSuperBlock(p_sb))!=0)
        return status;


    if(nInode > p_sb->itotal-1 || nInode<0) 
    	return -EINVAL;
  /*The inode must to be in use and belong to one of the legal file types.*/
    
    /* Read of the Inode to inode */
    if((status = soReadInode(&inode, nInode)) != 0) /*Validação de conformidade
                                                        • o número do nó-i tem que ser um valor válido
                                                       

                                                        Validação de consistência
                                                        • o nó-i referenciado tem que estar em uso e estar associado a um tipo válido
                                                        (ficheiro regular, directório ou atalho)*/
    	return status;
     

     
     /* align owner, group and other permissions bits to right and apply a mask
     * to the first 3 bits of each group 
     * 
     *       |owner|group|other|
     *  bit: |8|7|6|5|4|3|2|1|0|
     * perm: |r|w|x|r|w|x|r|w|x|
     * 
     */
    
    
    //------------------------------- pid = root -------------------------------   
     
     
    if((getuid() == 0) && (getgid() == 0)) 
    { 
        if((opRequested == R) || opRequested == W ) // R and W are always permited 
            return 0;  
             
        else {  // X only if in own, group ou other x exists
            if((inode.mode & ( X << 6 | X << 3 | X ) ) == ( X << 6 | X << 3 | X ))
                return 0;
                 
            else return -EACCES;            
            }
    }
    //-------------------------------_ pid ≠ root -------------------------------
     
    if(inode.owner == getuid())
    {
        /* usrid = own : R, W ou X são permitidos se em own houver,
respectivamente, um r, um w ou um x]*/
        if((inode.mode & opRequested << 6) != opRequested << 6)
            return -EACCES;
    }
    else
    {/* gid = group : R, W ou X são permitidos se em group houver,
respectivamente, um r, um w ou um x] */
        if(inode.group == getgid())
        {
            if((inode.mode & ( opRequested << 3) ) != ( opRequested << 3 ) )
                return -EACCES; 
        }
        else
        {/* usrid ≠ own e  gid ≠ group : R, W ou X são permitidos se em
other houver, respectivamente, um r, um w ou um x*/
            if((inode.mode & ( opRequested ) ) != ( opRequested ) )
                return -EACCES;
        }
    }
    return 0; 

     }
