/**
 *  \file soWrite.c (implementation file)
 *
 *  \author Tiago Bastos
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
 *  \brief Write data into an open regular file.
 *
 *  It tries to emulate <em>write</em> system call.
 *
 *  \param ePath path to the file
 *  \param buff pointer to the buffer where data to be written is stored
 *  \param count number of bytes to be written
 *  \param pos starting [byte] position in the file data continuum where data is to be written into
 *
 *  \return <em>number of bytes effectively written</em>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or or the path string is a \c NULL string or the path does
 *                      not describe an absolute path
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c EISDIR, if <tt>ePath</tt> describes a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EFBIG, if the file may grow passing its maximum size
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the file described by
 *                     <tt>ePath</tt>
 *  \return -\c ENOSPC, if there are no free data clusters
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soWrite (const char *ePath, void *buff, uint32_t count, int32_t pos)
{
  soColorProbe (230, "07;31", "soWrite (\"%s\", %p, %u, %u)\n", ePath, buff, count, pos);

  int status;
  SOSuperBlock *p_sb;
  uint32_t nInodeDir; // localização do numero do inode associado ao diretorio que tem a entrada que vai ser guardada
  uint32_t nInodeEnt; // localização do numero do inode associado a entrada que vai ser guardada
  uint32_t offset; // byte dentro do cluster de dados a escrever
  uint32_t clustInd; // posiçao da tabela de referencias diretas onde se encontra o cluster de dados onde esta o primeiro byte a escrever
  SOInode iNode;
  char buff_temp[BSLPC]; //buffer contendo os bytes residentes num determinado cluster
  char* aux;


  if((status = soLoadSuperBlock()) != 0)
  	return status;
  if((p_sb = soGetSuperBlock()) == NULL)
  	return -EIO;
  if((status = soQCheckSuperBlock(p_sb)) != 0)
  	return status;

  // Encontrar a entrada pelo caminho do ficheiro
  if((status = soGetDirEntryByPath(ePath,&nInodeDir,&nInodeEnt)) != 0)
  	return status;

  // se o ficheiro passar o tamanho maximo
  if((pos + count)>=MAX_FILE_SIZE)
  	return -EFBIG;

  // ler o nó-i da entrada
  if((status = soReadInode(&iNode,nInodeEnt)) != 0)
  	return status;
  // verificar se é um diretorio
  if((iNode.mode & INODE_TYPE_MASK) == INODE_DIR)
  	return -EISDIR;

  // o processo que invoca a operação tem que ter permissão de execução em todos os componentes de ePath
  if((status = soAccessGranted(nInodeEnt, W)) != 0){
  	if(status == -EACCES)
  		return -EPERM;
  	else
  		return status;
  }

  if(iNode.size < (pos+count))
  	iNode.size = (pos+count);

  if((status = soWriteInode(&iNode,nInodeEnt)) != 0)
  	return status;
  // converte a posição do byte no data continueem de um ficheiro, no index do elemento da lista de referencias diretas
  if((status = soConvertBPIDC(pos,&clustInd,&offset)) != 0)
  	return status;

  aux = buff;
  int wBtyes = 0;
  int i;

  for(i = 0; i < count; i++){
  	if(offset == BSLPC ){
  		if((status = soWriteFileCluster(nInodeEnt, clustInd, &buff_temp)) != 0)
  			return status;
  		clustInd++;
  		if((status = soReadFileCluster(nInodeEnt,clustInd,&buff_temp)) != 0)
  			return status;
  		offset = 0;
  	}

  	buff_temp[offset] = aux[i];
  	offset++;
  	wBtyes++;
  }

  if((status = soWriteFileCluster(nInodeEnt,clustInd,&buff_temp)) != 0)
  	return status;


  return wBtyes;
}
