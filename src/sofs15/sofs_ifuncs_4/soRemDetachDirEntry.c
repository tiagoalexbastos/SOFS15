/**
 *  \file soAddAttDirEntry.c (implementation file)
 *
 *  \author Tiago Bastos. " Ola eu sou o Tiago"
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>

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


/* Allusion to external functions */

int soGetDirEntryByName (uint32_t nInodeDir, const char *eName, uint32_t *p_nInodeEnt, uint32_t *p_idx);
int soCheckDirectoryEmptiness (uint32_t nInodeDir);

/** \brief operation remove a generic entry from a directory */
#define REM         0
/** \brief operation detach a generic entry from a directory */
#define DETACH      1

/**
 *  \brief Remove / detach a generic entry from a directory.
 *
 *  The entry whose name is <tt>eName</tt> is removed / detached from the directory associated with the inode whose
 *  number is <tt>nInodeDir</tt>. Thus, the inode must be in use and belong to the directory type.
 *
 *  Removal of a directory entry means exchanging the first and the last characters of the field <em>name</em>.
 *  Detachment of a directory entry means filling all the characters of the field <em>name</em with the \c NULL
 *  character.
 *
 *  The <tt>eName</tt> must be a <em>base name</em> and not a <em>path</em>, that is, it can not contain the
 *  character '/'. Besides there should exist an entry in the directory whose <em>name</em> field is <tt>eName</tt>.
 *
 *  Whenever the operation is removal and the type of the inode associated to the entry to be removed is of directory
 *  type, the operation can only be carried out if the directory is empty.
 *
 *  The <em>refcount</em> field of the inode associated to the entry to be removed / detached and, when required, of
 *  the inode associated to the directory are updated.
 *
 *  The file described by the inode associated to the entry to be removed / detached is only deleted from the file
 *  system if the <em>refcount</em> field becomes zero (there are no more hard links associated to it). In this case,
 *  the data clusters that store the file contents and the inode itself must be freed.
 *
 *  The process that calls the operation must have write (w) and execution (x) permissions on the directory.
 *
 *  \param nInodeDir number of the inode associated to the directory
 *  \param eName pointer to the string holding the name of the directory entry to be removed / detached
 *  \param op type of operation (REM / DETACH)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range or the pointer to the string is \c NULL or the
 *                      name string does not describe a file name or no operation of the defined class is described
 *  \return -\c ENAMETOOLONG, if the name string exceeds the maximum allowed length
 *  \return -\c ENOTDIR, if the inode type whose number is <tt>nInodeDir</tt> is not a directory
 *  \return -\c ENOENT,  if no entry with <tt>eName</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on the directory
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the directory
 *  \return -\c ENOTEMPTY, if the entry with <tt>eName</tt> describes a non-empty directory
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
 
int soRemDetachDirEntry (uint32_t nInodeDir, const char *eName, uint32_t op)
{
   soColorProbe (314, "07;31", "soRemDetachDirEntry (%"PRIu32", \"%s\", %"PRIu32")\n", nInodeDir, eName, op);
   
   SOSuperBlock *p_sb;              // ponteiro para superbloco
   SOInode inode, inodeDir;         // nó-i que contém directório, e nó-i associado a um directório
   SODataClust dirClust;            // cluster de dados
   int status;                         // variavel auxiliar para statusos de validação e consistência
   uint32_t nInodeEnt, index;       // nó-i de entrada; índice de entrada do directório;
   uint32_t nClustEnt, dirIndex;    // nº lógico do cluster; índice do directório no cluster
 

   if((status = soLoadSuperBlock()) != 0)
      return status;
   if((p_sb = soGetSuperBlock()) == NULL)
      return -EBADF;
 
   // Validar parametros de entrada e consistência
   if (strlen(eName) == 0 || eName == NULL)        // Verificar se eName não é nulo
      return -EINVAL;
   if ((strcmp(eName, ".") == 0) || (strcmp(eName, "..") == 0))   // Não se pode remover "." nem ".."
      return -EPERM;
   if (strlen(eName) > MAX_NAME)       // Verificar se nome do directório excede tamanho máximo permitido
      return -ENAMETOOLONG;
   if(op != REM && op != DETACH)              // Verificar se a operação solicitada é uma operação válida (Remoção/Anulação)
      return -EINVAL;
   if((strchr(eName,'/')) != 0)                    // Verificar se eName é basename (não pode conter '/')
      return -EINVAL;
   
   if(op != REM && op != DETACH)        // Verificar se a operação solicitada é uma operação válida (Remoção/Anulação)
    return -EINVAL;
   
   if((status = soReadInode(&inodeDir, nInodeDir)) != 0)     // Ler nó-i
      return status;
 
   if (!(inodeDir.mode & INODE_DIR))   // Verificar se o nó-i é directório
      return -ENOTDIR;
 
   if(inodeDir.mode & INODE_FREE)      // Verificar se nó-i está em uso
      return -EIUININVAL;
 
   if((status = soQCheckDirCont(p_sb, &inodeDir)) != 0)     // Verificar consistência do conteúdo do directório
      return -EDIRINVAL;
 
   // Verificar se existe permissão de escrita e execução
   if ((status = soAccessGranted(nInodeDir, X)) != 0)
      return status;
   if ((status = soAccessGranted (nInodeDir, W)) == -EACCES)
      return -EPERM;
   if(status != 0)
      return status;
   
   // Verifica se existe no directório uma entrada cujo nome é eName:
   // Se existir devolve o seu nó-i e índice da entrada do directório
   if ((status = soGetDirEntryByName(nInodeDir, eName, &nInodeEnt, &index)) != 0)
      return -ENOENT;
 
   if ((status = soReadInode(&inode, nInodeEnt)) != 0)   // Carregar o nó-i em uso que contém o directório eName
      return status;
 
   if ((inode.mode & INODE_TYPE_MASK) == INODE_DIR)
   {
      if (op == REM)                   // Se operação for Remover e nó-i for directório:
      {
         if((status = soCheckDirectoryEmptiness(nInodeEnt)) != 0)     // Se a entrada tiver directórios não pode ser apagada
                   return -ENOTEMPTY;
      }                                // Se operação for Detach:
         inodeDir.refcount--;          // Decrementar para libertar ".."
         inode.refcount--;             // Decrementar para libertar "."
   }
 
   nClustEnt = index/DPC;              // Número logico do cluster da entrada que vai ser apagada
   dirIndex = index%DPC;               // Índice do directório nesse cluster
   inode.refcount--;                   // Decrementar refcount
 
   if ((status = soReadFileCluster(nInodeDir, nClustEnt, &dirClust)) != 0)    // Ler o cluster
      return status;
 
   if(op == REM)                       // Remover:
   {
      dirClust.de[dirIndex].name[MAX_NAME] =  dirClust.de[dirIndex].name[0];
      dirClust.de[dirIndex].name[0] = '\0';
   }
 
   if(op == DETACH)                    // Anular:
   {
      memset(dirClust.de[dirIndex].name, '\0', MAX_NAME+1);   // Preencher com o caracter nulo ('\0') todos os elementos do campo name
      dirClust.de[dirIndex].nInode = NULL_INODE;              // Limpar DataCluster
   }
 
   if ((status = soWriteFileCluster (nInodeDir, nClustEnt, &dirClust)) != 0)     // Guardar o cluster com o directório apagado
      return status;
 
   if ((status = soWriteInode(&inode, nInodeEnt)) != 0)           // Escrever no nó-i que contém o directório
      return status;
 
   if (inode.refcount == 0)            // Se o nó-i não estiver associado a mais nenhuma entrada de directório:
   {
      if ((status = soHandleFileClusters(nInodeEnt, 0)) != 0)     // Libertar clusters do nó-i
         return status;
      if ((status = soFreeInode(nInodeEnt)) != 0)                 // Libertar nó-i
         return status;
   }
 
   if ((status = soWriteInode(&inodeDir, nInodeDir)) != 0)        // Escrever no nó-i que contém a entrada do directorio
      return status;
 
  return 0;
}
