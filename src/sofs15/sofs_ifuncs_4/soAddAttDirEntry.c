/**
 *  \file soAddAttDirEntry.c (implementation file)
 *
 *  \author João Rodrigues
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>
#include <stdlib.h>

#include "sofs_probe.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_direntry.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_1.h"
#include "sofs_ifuncs_2.h"
#include "sofs_ifuncs_3.h"

/* Allusion to external function */

int soGetDirEntryByName (uint32_t nInodeDir, const char *eName, uint32_t *p_nInodeEnt, uint32_t *p_idx);

/** \brief operation add a generic entry to a directory */
#define ADD         0
/** \brief operation attach an entry to a directory to a directory */
#define ATTACH      1

/**
 *  \brief Add a generic entry / attach an entry to a directory to a directory.
 *
 *  In the first case, a generic entry whose name is <tt>eName</tt> and whose inode number is <tt>nInodeEnt</tt> is added
 *  to the directory associated with the inode whose number is <tt>nInodeDir</tt>. Thus, both inodes must be in use and
 *  belong to a legal type, the former, and to the directory type, the latter.
 *
 *  Whenever the type of the inode associated to the entry to be added is of directory type, the directory is initialized
 *  by setting its contents to represent an empty directory.
 *
 *  In the second case, an entry to a directory whose name is <tt>eName</tt> and whose inode number is <tt>nInodeEnt</tt>
 *  is attached to the directory, the so called <em>base directory</em>, associated to the inode whose number is
 *  <tt>nInodeDir</tt>. The entry to be attached is supposed to represent itself a fully organized directory, the so
 *  called <em>subsidiary directory</em>. Thus, both inodes must be in use and belong to the directory type.
 *
 *  The <tt>eName</tt> must be a <em>base name</em> and not a <em>path</em>, that is, it can not contain the
 *  character '/'. Besides there should not already be any entry in the directory whose <em>name</em> field is
 *  <tt>eName</tt>.
 *
 *  The <em>refcount</em> field of the inode associated to the entry to be added / updated and, when required, of the
 *  inode associated to the directory are updated. This may also happen to the <em>size</em> field of either or both
 *  inodes.
 *
 *  The process that calls the operation must have write (w) and execution (x) permissions on the directory.
 *
 *  \param nInodeDir number of the inode associated to the directory
 *  \param eName pointer to the string holding the name of the entry to be added / attached
 *  \param nInodeEnt number of the inode associated to the entry to be added / attached
 *  \param op type of operation (ADD / ATTACH)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if any of the <em>inode numbers</em> are out of range or the pointer to the string is \c NULL
 *                      or the name string does not describe a file name or no operation of the defined class is described
 *  \return -\c ENAMETOOLONG, if the name string exceeds the maximum allowed length
 *  \return -\c ENOTDIR, if the inode type whose number is <tt>nInodeDir</tt> (ADD), or both the inode types (ATTACH),
 *                       are not directories
 *  \return -\c EEXIST, if an entry with the <tt>eName</tt> already exists
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on the directory where the
 *                      entry is to be added / attached
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the directory where the entry
 *                     is to be added / attached
 *  \return -\c EMLINK, if the maximum number of hardlinks in either one of inodes has already been attained
 *  \return -\c EFBIG, if the directory where the entry is to be added / attached, has already grown to its maximum size
 *  \return -\c ENOSPC, if there are no free data clusters
 *  \return -\c EDIRINVAL, if the directory is inconsistent
 *  \return -\c EDEINVAL, if the directory entry is inconsistent
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soAddAttDirEntry (uint32_t nInodeDir, const char *eName, uint32_t nInodeEnt, uint32_t op)
{
  soColorProbe (313, "07;31", "soAddAttDirEntry (%"PRIu32", \"%s\", %"PRIu32", %"PRIu32")\n", nInodeDir,
                eName, nInodeEnt, op);

  /*
    testar nome (tamanho e não pode conter o carater '/')
    testar no do diretorio(ver se e diretorio)

    ADD{
      //utilizado para adicionar ficheiros e diretorios vazios
      testar no do ficheiro
      se for diretorio{
        verificar se e um diretorio vazio
        alocar cluster no diretorio vazio
        atualizar entrada "." ".."
      }
      encontrar entrada vazia 
      se cluster cheio{
        alocar data cluster
      }
    }

    ATTACH{
      //utilizado para adicionar diretorios
      atualizar entrada ".."
    }
    ver se cluster onde esta a entrada livre esta alocado
    buscar cluster da entrada livre
    escrever entrada
    incrementar refcount
  */

  int stat,i,j;
  SOInode iNodeDir, iNodeEnt;


  //verificação do nome
  if(eName == NULL || strrchr(eName, '/') != NULL) return -EINVAL;
  if(strlen(eName) > MAX_NAME) return -ENAMETOOLONG;


  //no-i
  if((stat = soAccessGranted (nInodeDir, W)) != 0) return stat;
  if((stat = soAccessGranted (nInodeDir, X)) != 0) return stat;

  if((stat = soReadInode (&iNodeEnt, nInodeEnt)) != 0) {return stat;}//teste ao no-i que vai ser ligado
  if((stat = soReadInode (&iNodeDir, nInodeDir)) != 0) {return stat;}

  if((iNodeDir.mode & INODE_DIR) == 0) {return -ENOTDIR;}  //verificar se diretorio pai e diretorio

  if((iNodeEnt.mode & INODE_DIR) != 0){
  	if((stat = soAccessGranted (nInodeEnt, R)) != 0) return stat;
  	if((stat = soAccessGranted (nInodeEnt, W)) != 0) return stat;
  }

  uint32_t index,trash; //index: posição livre
  if((stat = soGetDirEntryByName(nInodeDir,eName,&trash,&index)) != -ENOENT && stat != 0) {
//  printf("erro 143\n"); 
    return stat;
  }    //encontrar primeira entrada livre
  else if (stat == 0) {return -EEXIST;} 

  if(op == ADD){
    if ((iNodeEnt.mode & INODE_DIR) != 0){
      if (iNodeEnt.refcount != 0) {return -EDIRINVAL;}

      uint32_t logicCluster;
      
      SODataClust entDirClust; //data cluster diretorio vazio
      if((stat = soHandleFileCluster(nInodeEnt, 0, ALLOC, &logicCluster)) != 0) {
        return stat;
      }  //alocar cluster no diretorio vazio
      if((stat = soReadFileCluster(nInodeEnt, 0, &entDirClust)) != 0) {
        return stat;
      } // ler cluster 0 do diretorio vazio

      if((stat = soReadInode (&iNodeEnt, nInodeEnt)) != 0) {
        return stat;
      }//atualizar na memoria o no-i do diretorio vazio

      for (i = 0; i < DPC; i++)                                     // apagar as entradas do directorio
      {                                                             // colocando o 1º caracter do campo name como '\0'
        for(j = 0; j<=MAX_NAME; j++){
          entDirClust.de[i].name[j] = '\0';
        }
        entDirClust.de[i].nInode = NULL_INODE;                     // colocar o número do iNode a Null
      }
      
      //entrada "."
      strcpy((char *)entDirClust.de[0].name, ".");
      entDirClust.de[0].nInode = nInodeEnt;

      //entrada ".."
      strcpy((char *)entDirClust.de[1].name, ".."); //nome da entrada ".." do diretorio vazio = nome da entrada "." do diretorio destino
      entDirClust.de[1].nInode = nInodeDir;

      if((stat = soWriteFileCluster (nInodeEnt, 0, &entDirClust))){
        return stat;
      } //escrever cluster

      
      iNodeEnt.refcount ++;  //referencias diretorio vazio
      iNodeEnt.size = 2048; //tamanho do diretorio em bytes

      if((stat = soWriteInode (&iNodeEnt, nInodeEnt))){
        return stat;
      }  //escrever no-i diretorio vazio

      iNodeDir.refcount ++;  //referencias diretorio pai   

      if((stat = soWriteInode (&iNodeDir, nInodeDir))){
        return stat;
      }  //escrever no-i diretorio pai
    }

  }
  else if(op == ATTACH){
    if((iNodeEnt.mode & INODE_DIR) != 0 && iNodeEnt.refcount == 0) {return -EDIRINVAL;}

    if((iNodeEnt.mode & INODE_DIR) != 0){
      SODataClust entDirClust;  //data cluster diretorio filho
      if((stat = soReadFileCluster(nInodeEnt, 0, &entDirClust)) != 0) {
        return stat;
      } // ler cluster 0 do diretorio filho

      //entrada ".."
      entDirClust.de[1].nInode = nInodeDir;

      if((stat = soWriteFileCluster (nInodeEnt, 0, &entDirClust))){
        return stat;
      } //escrever cluster

      iNodeDir.refcount ++;  //referencias diretorio pai   

      if((stat = soWriteInode (&iNodeDir, nInodeDir))){
        return stat;
      }  //escrever no-i diretorio pai
    }
    else 
      return -ENOTDIR;
  }
  
  else{
    return -EINVAL;
  }

  SODataClust dirClust;     //data cluster diretorio pai
  uint32_t inCluster;

  //verificar se o cluster esta alocado 
  if((stat = soHandleFileCluster (nInodeDir, index / DPC, GET, &inCluster)) != 0){
    return stat;
  }
  if(inCluster == NULL_CLUSTER){
    if((stat = soHandleFileCluster (nInodeDir, index / DPC, ALLOC, &inCluster)) != 0){
      return stat;
    }

    if((stat = soReadFileCluster(nInodeDir, index / DPC, &dirClust)) != 0){
      return stat;
    }

    for (i = 0; i < DPC; i++)                                     // apagar as entradas do directorio
    {                                                             // colocando o 1º caracter do campo name como '\0'
      for(j = 0; j<=MAX_NAME; j++){
        dirClust.de[i].name[j] = '\0';
      }
      dirClust.de[i].nInode = NULL_INODE;                     // colocar o número do iNode a Null
    }

   if((stat = soWriteFileCluster(nInodeDir, index / DPC, &dirClust)) != 0){
      return stat;
    }
    
    if((stat = soReadInode (&iNodeDir, nInodeDir)) != 0) {
      return stat;
    }//atualizar na memoria o no-i do diretorio
    iNodeDir.size += 2048;
    if((stat = soWriteInode (&iNodeDir, nInodeDir))){
      return stat;
    }  //escrever no-i diretorio
  }

  if((stat = soReadFileCluster  (nInodeDir , index / DPC , &dirClust) != 0)){
    return stat;
  }

  memset(dirClust.de[index%DPC].name,'\0',MAX_NAME+1); //apagar nome antigo

  strcpy((char *)dirClust.de[index%DPC].name,eName);
  dirClust.de[index%DPC].nInode = nInodeEnt;

  if((stat = soWriteFileCluster (nInodeDir, index / DPC, &dirClust))){
    return stat;
  } //escrever cluster

  iNodeEnt.refcount ++;

  if((stat = soWriteInode (&iNodeEnt, nInodeEnt))){
    return stat;
  }  //escrever no-i do ficheiro

  return 0;
}
