/**
 *  \file soGetDirEntryByName.c (implementation file)
 *
 *  \author ---
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

/**
 *  \brief Get an entry by name.
 *
 *  The directory contents, seen as an array of directory entries, is parsed to find an entry whose name is
 *  <tt>eName</tt>. Thus, the inode associated to the directory must be in use and belong to the directory type.
 *
 *  The <tt>eName</tt> must also be a <em>base name</em> and not a <em>path</em>, that is, it can not contain the
 *  character '/'.
 *
 *  The process that calls the operation must have execution (x) permission on the directory.
 *
 *  \param nInodeDir number of the inode associated to the directory
 *  \param eName pointer to the string holding the name of the directory entry to be located
 *  \param p_nInodeEnt pointer to the location where the number of the inode associated to the directory entry whose
 *                     name is passed, is to be stored
 *                     (nothing is stored if \c NULL)
 *  \param p_idx pointer to the location where the index to the directory entry whose name is passed, or the index of
 *               the first entry that is free, is to be stored
 *               (nothing is stored if \c NULL)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range or the pointer to the string is \c NULL or the
 *                      name string does not describe a file name
 *  \return -\c ENAMETOOLONG, if the name string exceeds the maximum allowed length
 *  \return -\c ENOTDIR, if the inode type is not a directory
 *  \return -\c ENOENT,  if no entry with <tt>name</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on the directory
 *  \return -\c EDIRINVAL, if the directory is inconsistent
 *  \return -\c EDEINVAL, if the directory entry is inconsistent
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
 

int soGetDirEntryByName (uint32_t nInodeDir, const char *eName, uint32_t *p_nInodeEnt, uint32_t *p_idx)
{
  soColorProbe (312, "07;31", "soGetDirEntryByName (%"PRIu32", \"%s\", %p, %p)\n",
                nInodeDir, eName, p_nInodeEnt, p_idx);
  char auxString[MAX_NAME+1];         // String auxiliar para verificar se "eName" descreve um ficheiro
  int status;                         // Variável de verificação de erros
  int flag = -1;                      // Flag para sinalização da primeira posição livre
  uint32_t indDataClust;              // Variável para testar os clusters de dados envolvidos na busca da entrada "eName"
  uint32_t indDirEntry;               // Variável para testar as entradas de directório envolvidas na busca da entrada "eName"
  SOInode inode;                      // Criação de uma estrutura do tipo "SOInode" 
  SOSuperBlock *p_sb;                 // Ponteiro para o superbloco 
  SODirEntry dirEntry[DPC];           // Array de estruturas do tipo SODirEntry 
  uint32_t nClusters;                 // Variável auxiliar

  if((status=soLoadSuperBlock())!=0) return status;          // carregar super bloco na memoria principal
  if((p_sb=soGetSuperBlock())==NULL) return -ELIBBAD;  // obter ponteiro para o superbloco
  if((status=soQCheckSuperBlock(p_sb))!=0) return status;    // verificar consistencia do superbloco


  //* O ponteiro para a string "eName" não poderá ser nulo */
   if(eName == NULL) return -EINVAL;                       

  /* O comprimento da string "eName" terá que ser diferente de zero */
  if(strlen (eName) == 0) return -EINVAL;                 
 
   /* O comprimento da string "eName" não poderá ser superior ao tamanho permitido */ 
  if(strlen (eName) > MAX_NAME) return -ENAMETOOLONG;            

   /* A string "eName" tem de ter um nome do tipo ficheiro */
  strcpy(auxString,eName);
  strcpy(auxString,basename(auxString));
  if(strcmp(auxString,eName)!= 0) return -EINVAL;
      
	
		  
  if((status=soReadInode(&inode,nInodeDir)) != 0) return status; // guardar o Inode em memoria previamente alocada
  if((status=soAccessGranted(nInodeDir,X)) != 0) return status; // verificar permissao de execucao
		 
 /* O nó de índice "nInodeDir" terá que ser do tipo "directório" */
  if((inode.mode & INODE_DIR) == 0) return -ENOTDIR;   
  if((status=soQCheckDirCont(p_sb, &inode))!=0) return status;   // verifica a consistencia do directorio
	 
	
 
 
	nClusters = inode.size / (sizeof(SODirEntry)*DPC);         // Nº de clusters associados ao nó "inode"
  
   for(indDataClust = 0;indDataClust < nClusters;indDataClust++)
   {
      /* Uso da função "soReadFileCluster" para se ler o cluster de índice "indDataClust"
       do nó de índice "nInodeDir" que poderá conter várias entradas de directórios */   
      if((status = soReadFileCluster (nInodeDir,indDataClust,dirEntry))!= 0)  return status;                

      for(indDirEntry = 0;indDirEntry < DPC;indDirEntry++)        // Leitura das entradas de directório do cluster de índice "indDataClust"
      {
        /* Se o nome da entrada analisada for igual ao nome que foi fornecido,é preenchida a variável "p_nInodeEnt"
        que é um ponteiro para essa entrada */
          if(strcmp (eName, (char *) dirEntry[indDirEntry].name) == 0)
          {
            /* Nada é armazenado se o ponteiro "p_nInodeEnt for nulo */
            if(p_nInodeEnt != NULL)
             *p_nInodeEnt = dirEntry[indDirEntry].nInode;        // Ponteiro para o nó que contém a entrada "eName"
        
            /* Nada é armazenado se o ponteiro "p_idx" for nulo */
            if(p_idx != NULL)
              *p_idx = indDataClust * DPC + indDirEntry;          // Ponteiro para o índice da entrada "eName"
           
            return 0;                             // O programa é terminado com sucesso
          }

         if(dirEntry[indDirEntry].name[0] == '\0' && (flag == -1))
           flag = indDataClust * DPC + indDirEntry;        // Índice da 1º Entrada Livre 
         }
   }
   
   /* Não foi encontrada nenhuma entrada com o nome "eName" */
   if(flag!= -1){
      /* Nada é armazenado se o ponteiro "p_idx" for nulo */
      if(p_idx!= NULL)
        *p_idx = flag;                            // Índice da 1º Entrada Livre
   }
   else /* Não há nenhuma posição livre */
   {
     /* Nada é armazenado se o ponteiro "p_idx" for nulo */
     if(p_idx!= NULL)
     *p_idx = indDataClust * DPC;
   }
   return -ENOENT;                                // Não há nenhuma entrada com o nome fornecido
}


