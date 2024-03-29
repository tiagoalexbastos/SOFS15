/**
 *  \file soRead.c (implementation file)
 *
 *  \author ---
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <libgen.h>
#include <string.h>

#include "sofs_probe.h"
#include "sofs_const.h"
#include "sofs_rawdisk.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_direntry.h"
#include "sofs_datacluster.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_1.h"
#include "sofs_ifuncs_2.h"
#include "sofs_ifuncs_3.h"
#include "sofs_ifuncs_4.h"

/**
 *  \brief Read data from an open regular file.
 *
 *  It tries to emulate <em>read</em> system call.
 *
 *  \param ePath path to the file
 *  \param buff pointer to the buffer where data to be read is to be stored
 *  \param count number of bytes to be read
 *  \param pos starting [byte] position in the file data continuum where data is to be read from
 *
 *  \return <em>number of bytes effectively read</em>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or or the path string is a \c NULL string or the path does
 *                      not describe an absolute path
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c EISDIR, if <tt>ePath</tt> describes a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EFBIG, if the starting [byte] position in the file data continuum assumes a value passing its maximum
 *                     size
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EPERM, if the process that calls the operation has not read permission on the file described by
 *                     <tt>ePath</tt>
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soRead (const char *ePath, void *buff, uint32_t count, int32_t pos)
{

  soColorProbe (229, "07;31", "soRead (\"%s\", %p, %u, %u)\n", ePath, buff, count, pos);
  int stat;
  uint32_t nInode,nInodeDir,nBlk,off;
  int transfer = 0;         //n de bytes transferidos
  SOInode iNode;

  if ((stat = soGetDirEntryByPath(ePath,&nInodeDir,&nInode)) != 0){
    return stat;
  }                //obter n do no i do ficheiro

  if((stat = soAccessGranted(nInode, R)) != 0){
    return stat;
  }   //avaliar acesso de leitura do ficheiro

  if ((stat = soReadInode(&iNode,nInode)) != 0){
    return stat;
  }         //obter no i do ficheiro

  //verificar se ficheiro não é um diretório

  if((iNode.mode & INODE_TYPE_MASK) == INODE_DIR)
    return -EISDIR;

  if (count + pos > iNode.size)
    count = iNode.size - pos;                        // atualizar o n de bytes a ser transferidos se o ultimo byte estiver fora do limite do ficheiro

  while(count > 0){
    if((stat = soConvertBPIDC(pos,&nBlk,&off)) != 0){
            return stat;
    }        //obter posição no continuum

    SODataClust cluster;
    if((stat = soReadFileCluster(nInode, nBlk, &cluster)) != 0){
            return stat;
    }                //ler cluster

    if(count > CLUSTER_SIZE - off)
            memcpy(buff+transfer, &cluster.data[off], CLUSTER_SIZE - off); //copiar porcao do cluster
    else
            memcpy(buff+transfer, &cluster.data[off], count);

    transfer += CLUSTER_SIZE - off;
    count -= CLUSTER_SIZE - off;
    pos += CLUSTER_SIZE - off;
	}  

  return transfer;

}

