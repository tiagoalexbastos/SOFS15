/**
 *  \file mkfs_sofs15.c (implementation file)
 *
 *  \brief The SOFS15 formatting tool.
 *
 *  It stores in predefined blocks of the storage device the file system metadata. With it, the storage device may be
 *  envisaged operationally as an implementation of SOFS15.
 *
 *  The following data structures are created and initialized:
 *     \li the superblock
 *     \li the table of inodes
 *     \li the data zone
 *     \li the contents of the root directory seen as empty.
 *
 *  SINOPSIS:
 *  <P><PRE>                mkfs_sofs15 [OPTIONS] supp-file
 *
 *                OPTIONS:
 *                 -n name --- set volume name (default: "SOFS15")
 *                 -i num  --- set number of inodes (default: N/8, where N = number of blocks)
 *                 -z      --- set zero mode (default: not zero)
 *                 -q      --- set quiet mode (default: not quiet)
 *                 -h      --- print this help.</PRE>
 *
 *  \author Artur Carneiro Pereira - September 2008
 *  \author Miguel Oliveira e Silva - September 2009
 *  \author António Rui Borges - September 2010 - August 2015
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "sofs_const.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_direntry.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"

/* Allusion to internal functions */

static int fillInSuperBlock (SOSuperBlock *p_sb, uint32_t ntotal, uint32_t itotal, uint32_t fcblktotal,
		                     uint32_t nclusttotal, unsigned char *name);
static int fillInINT (SOSuperBlock *p_sb);
static int fillInRootDir (SOSuperBlock *p_sb);
static int fillInTRefFDC (SOSuperBlock *p_sb, int zero);
static int checkFSConsist (void);
static void printUsage (char *cmd_name);
static void printError (int errcode, char *cmd_name);

/* The main function */

int main (int argc, char *argv[])
{
  char *name = "SOFS15";                         /* volume name */
  uint32_t itotal = 0;                           /* total number of inodes, if kept, set value automatically */
  int quiet = 0;                                 /* quiet mode, if kept, set not quiet mode */
  int zero = 0;                                  /* zero mode, if kept, set not zero mode */

  /* process command line options */

  int opt;                                       /* selected option */

  do
  { switch ((opt = getopt (argc, argv, "n:i:qzh")))
    { case 'n': /* volume name */
                name = optarg;
                break;
      case 'i': /* total number of inodes */
                if (atoi (optarg) < 0)
                   { fprintf (stderr, "%s: Negative inodes number.\n", basename (argv[0]));
                     printUsage (basename (argv[0]));
                     return EXIT_FAILURE;
                   }
                itotal = (uint32_t) atoi (optarg);
                break;
      case 'q': /* quiet mode */
                quiet = 1;                       /* set quiet mode for processing: no messages are issued */
                break;
      case 'z': /* zero mode */
                zero = 1;                        /* set zero mode for processing: the information content of all free
                                                    data clusters are set to zero */
                break;
      case 'h': /* help mode */
                printUsage (basename (argv[0]));
                return EXIT_SUCCESS;
      case -1:  break;
      default:  fprintf (stderr, "%s: Wrong option.\n", basename (argv[0]));
                printUsage (basename (argv[0]));
                return EXIT_FAILURE;
    }
  } while (opt != -1);
  if ((argc - optind) != 1)                      /* check existence of mandatory argument: storage device name */
     { fprintf (stderr, "%s: Wrong number of mandatory arguments.\n", basename (argv[0]));
       printUsage (basename (argv[0]));
       return EXIT_FAILURE;
     }

  /* check for storage device conformity */

  char *devname;                                 /* path to the storage device in the Linux file system */
  struct stat st;                                /* file attributes */

  devname = argv[optind];
  if (stat (devname, &st) == -1)                 /* get file attributes */
     { printError (-errno, basename (argv[0]));
       return EXIT_FAILURE;
     }
  if (st.st_size % BLOCK_SIZE != 0)              /* check file size: the storage device must have a size in bytes
                                                    multiple of block size */
     { fprintf (stderr, "%s: Bad size of support file.\n", basename (argv[0]));
       return EXIT_FAILURE;
     }


  /* evaluating the file system architecture parameters
   * full occupation of the storage device when seen as an array of blocks supposes that the equation bellow
   *
   *    NTBlk = 1 + sige(NTClt/RPB) + NBlkTIN + NTClt*BLOCKS_PER_CLUSTER
   *
   *    where NTBlk means total number of blocks
   *          NTClt means total number of clusters of the data zone
   *          RPB means total number of references to clusters which can be stored in a block
   *          NBlkTIN means total number of blocks required to store the inode table
   *          BLOCKS_PER_CLUSTER means number of blocks which fit in a cluster
   *          sige (.) means the smallest integer greater or equal to the argument
   *
   * has integer solutions
   * this is not always true, so a final adjustment may be made to the parameter NBlkTIN to warrant this
   * furthermore, since the equation is non-linear, the procedure to solve it requires several steps
   * (in this case three)
   */

  uint32_t ntotal;                               /* total number of blocks */
  uint32_t iblktotal;                            /* number of blocks of the inode table */
  uint32_t nclusttotal;                          /* total number of clusters */
  uint32_t fcblktotal;                           /* number of blocks of the table of references to data clusters */
  uint32_t tmp;                                  /* temporary variable */

  ntotal = st.st_size / BLOCK_SIZE;
  if (itotal == 0) itotal = ntotal >> 3;         /* use the default value */
  if ((itotal % IPB) == 0)
     iblktotal = itotal / IPB;
     else iblktotal = itotal / IPB + 1;
                                                 /* step number 1 */
  tmp = (ntotal - 1 - iblktotal) / BLOCKS_PER_CLUSTER;
  if ((tmp % RPB) == 0)
	 fcblktotal = tmp / RPB;
     else fcblktotal = tmp / RPB + 1;
                                                 /* step number 2 */
  nclusttotal = (ntotal - 1 - iblktotal - fcblktotal) / BLOCKS_PER_CLUSTER;
  if ((nclusttotal % RPB) == 0)
	 fcblktotal = nclusttotal / RPB;
     else fcblktotal = nclusttotal / RPB + 1;
                                                 /* step number 3 */
  if ((nclusttotal % RPB) != 0)
     { if ((ntotal - 1 - iblktotal - fcblktotal - nclusttotal * BLOCKS_PER_CLUSTER) >= BLOCKS_PER_CLUSTER)
          nclusttotal += 1;
     }
                                                 /* final adjustment */
  iblktotal = ntotal - 1 - fcblktotal - nclusttotal * BLOCKS_PER_CLUSTER;
  itotal = iblktotal * IPB;

  /* formatting of the storage device is going to start */

  SOSuperBlock *p_sb;                            /* pointer to the superblock */
  int status;                                    /* status of operation */

  if (!quiet)
     printf("\e[34mInstalling a %"PRIu32"-inodes SOFS15 file system in %s.\e[0m\n", itotal, argv[optind]);

  /* open a buffered communication channel with the storage device */

  if ((status = soOpenBufferCache (argv[optind], BUF)) != 0)
     { printError (status, basename (argv[0]));
       return EXIT_FAILURE;
     }

  /* read the contents of the superblock to the internal storage area
   * this operation only serves at present time to get a pointer to the superblock storage area in main memory
   */

  if ((status = soLoadSuperBlock ()) != 0)
     return status;
  p_sb = soGetSuperBlock ();

  /* filling in the superblock fields:
   *   magic number should be set presently to 0xFFFF, this enables that if something goes wrong during formating, the
   *   device can never be mounted later on
   */

  if (!quiet)
     { printf ("Filling in the superblock fields ... ");
       fflush (stdout);                          /* make sure the message is printed now */
     }

  if ((status = fillInSuperBlock (p_sb, ntotal, itotal, fcblktotal, nclusttotal, (unsigned char *) name)) != 0)
     { printError (status, basename (argv[0]));
       soCloseBufferCache ();
       return EXIT_FAILURE;
     }

  if (!quiet) printf ("done.\n");

  /* filling in the inode table:
   *   only inode 0 is in use (it describes the root directory)
   */

  if (!quiet)
     { printf ("Filling in the table of inodes ... ");
       fflush (stdout);                          /* make sure the message is printed now */
     }

  if ((status = fillInINT (p_sb)) != 0)
     { printError (status, basename (argv[0]));
       soCloseBufferCache ();
       return EXIT_FAILURE;
     }

  if (!quiet) printf ("done.\n");

  /* filling in the contents of the root directory:
   *   the first 2 entries are filled in with "." and ".." references
   *   the other entries are kept empty
   */

  if (!quiet)
     { printf ("Filling in the contents of the root directory ... ");
       fflush (stdout);                          /* make sure the message is printed now */
     }

  if ((status = fillInRootDir (p_sb)) != 0)
     { printError (status, basename (argv[0]));
       soCloseBufferCache ();
       return EXIT_FAILURE;
     }

  if (!quiet) printf ("done.\n");

  /*
   * create the table of references to free data clusters as a static linear FIFO
   * zero fill the remaining data clusters if full formating was required:
   *   zero mode was selected
   */

  if (!quiet)
     { printf ("Filling in the contents of the table of references to free data clusters ... ");
       fflush (stdout);                          /* make sure the message is printed now */
     }

  if ((status = fillInTRefFDC (p_sb, zero)) != 0)
     { printError (status, basename (argv[0]));
       soCloseBufferCache ();
       return EXIT_FAILURE;
     }

  if (!quiet) printf ("done.\n");

  /* magic number should now be set to the right value before writing the contents of the superblock to the storage
     device */

  p_sb->magic = MAGIC_NUMBER;
  if ((status = soStoreSuperBlock ()) != 0)
     return status;

  /* check the consistency of the file system metadata */

  if (!quiet)
     { printf ("Checking file system metadata... ");
       fflush (stdout);                          /* make sure the message is printed now */
     }

  if ((status = checkFSConsist ()) != 0)
     { fprintf(stderr, "error # %d - %s\n", -status, soGetErrorMessage (-status));
       soCloseBufferCache ();
       return EXIT_FAILURE;
     }

  if (!quiet) printf ("done.\n");

  /* close the unbuffered communication channel with the storage device */

  if ((status = soCloseBufferCache ()) != 0)
     { printError (status, basename (argv[0]));
       return EXIT_FAILURE;
     }

  /* that's all */

  if (!quiet) printf ("Formating concluded.\n");

  return EXIT_SUCCESS;

} /* end of main */

/*
 * print help message
 */

static void printUsage (char *cmd_name)
{
  printf ("Sinopsis: %s [OPTIONS] supp-file\n"
          "  OPTIONS:\n"
          "  -n name --- set volume name (default: \"SOFS15\")\n"
          "  -i num  --- set number of inodes (default: N/8, where N = number of blocks)\n"
          "  -z      --- set zero mode (default: not zero)\n"
          "  -q      --- set quiet mode (default: not quiet)\n"
          "  -h      --- print this help\n", cmd_name);
}

/*
 * print error message
 */

static void printError (int errcode, char *cmd_name)
{
  fprintf(stderr, "%s: error #%d - %s\n", cmd_name, -errcode,
          soGetErrorMessage (-errcode));
}

  /* filling in the superblock fields:
   *   magic number should be set presently to 0xFFFF, this enables that if something goes wrong during formating, the
   *   device can never be mounted later on
   */

static int fillInSuperBlock (SOSuperBlock *p_sb, uint32_t ntotal, uint32_t itotal, uint32_t fcblktotal,
		                     uint32_t nclusttotal, unsigned char *name)
{  
   
  if(p_sb==NULL) return -EINVAL;
   
  int i;

  /* HEADER */
  p_sb->magic = MAGIC_NUMBER; /* magic number - file system id number */
  p_sb->version = VERSION_NUMBER; /* version number */
  
  /*name*/
  unsigned int l = 0;
  while (name[l] != '\0' && l < PARTITION_NAME_SIZE) {
      p_sb->name[l] = name[l];
      l++;
  }
  p_sb->name[l] = '\0'; // string terminator
  /*end name*/

  p_sb->ntotal = ntotal; /* total number of blocks in the device */
  p_sb->mstat = PRU; /* flag signaling if the file system was properly unmounted the last time it was mounted. PRU - Properly Unmounted */


  /* I-TABLE */
  p_sb->itable_start = 1; /* physical number of the block where the table of inodes starts - block 0 is for SB, so 1 is for iNode table*/
  p_sb->itable_size = itotal/IPB ; /* number of blocks that the table of inodes comprises (number of inodes/inodes per block) */
  p_sb->itotal = itotal; /* total number of inodes */
  p_sb->ifree = itotal-1; /* number of free inodes */
  p_sb->ihdtl = 1; /* index of the array element that forms the head/tail of the double-linked list of free inodes (point of retrieval/insertion) */

  
  /* FREE DATA CLUSTERS TABLE */
  p_sb->tbfreeclust_start = p_sb->itable_start + p_sb->itable_size; /* physical number of the block where the table of references to free data clusters starts */
  p_sb->tbfreeclust_size = fcblktotal; /* number of blocks that the table of references to free data clusters comprises */
  p_sb->tbfreeclust_head = 1; /* index of the array element that forms the head of the table of references to free data clusters (point of retrieval) */
  p_sb->tbfreeclust_tail = 0; /* index of the array element that forms the tail of the table of references to free data clusters (point of insertion) */

  /* DATA ZONE */
  p_sb->dzone_start = p_sb->itable_start + p_sb->itable_size + fcblktotal;/* physical number of the block where the data zone starts (physical number of the first data cluster) */
  p_sb->dzone_total = nclusttotal; /* total number of data clusters */
  p_sb->dzone_free = nclusttotal-1; /* number of free data clusters */


  /*Retrieval Cache*/
  p_sb->dzone_retriev.cache_idx=DZONE_CACHE_SIZE; /* retrieval cache of references to free data clusters */
  for(i = 0; i < DZONE_CACHE_SIZE; i++) /* retrieval cache is empty */
    p_sb->dzone_retriev.cache[i] = NULL_CLUSTER;/*fill cache with default value as seen in class with showblock*/

  /*Insertion Cache*/
  p_sb->dzone_insert.cache_idx=0; /* insertion cache of references to free data clusters */
  for(i = 0; i < DZONE_CACHE_SIZE; i++) /* insertion cache is empty */
          p_sb->dzone_insert.cache[i] = NULL_CLUSTER;

  /* RESERVED ZONE */ 
  for (i = 0; i < BLOCK_SIZE - PARTITION_NAME_SIZE - 1 - 16 * sizeof(uint32_t) - 2 * sizeof(struct fCNode); i++)
          p_sb->reserved[i] = 0xee; // 0xEE was suggested by prof Borges

  int stat; // function return control

  if ((stat = soStoreSuperBlock()) != 0)
      return stat;

  return 0;
}

/*
 * filling in the inode table:
 *   only inode 0 is in use (it describes the root directory)
 */

static int fillInINT (SOSuperBlock *p_sb)
{
  SOInode *iNode; /* Pointer to contents of Inode     */
  
  uint32_t block, inode, i;       /* variables to use in cycles. block for each block, inode for
                           each inode and i is used in the array of direct references in inode table */
  int status;             /* variable to return the errors if they happen     */
     
    /* Validate Arguments */
    if(p_sb == NULL)
        return -EINVAL;           //it's used when the pointer is null(check any function. Ex: sofs_ifuncs_1.h)    
 
    /*  Load the contents of all blocks of the table of inodes from devide storage  */
    /*          into internal storage and fill them as free inodes      */
 

    for( block = p_sb->itable_start; block < (p_sb->itable_size + p_sb->itable_start); block++ ) {
        if((status = soLoadBlockInT(block - p_sb->itable_start)) != 0)   /*Load the contents of a specific block of the table of inodes into internal storage. */
            return status;                                               // location: sofs_basicoper.c
         
        iNode = soGetBlockInT();            /* Get a pointer to the content */
         
        for( inode = 0; inode < IPB; inode++ ) { /* Put all inode of the block in free state */
            iNode[inode].mode = INODE_FREE;     /* flag signaling inode is free */
            iNode[inode].refcount = 0;
            iNode[inode].owner = 0;
            iNode[inode].group = 0;
            iNode[inode].size = 0;
            iNode[inode].clucount = 0;
            iNode[inode].vD1.prev = ((block-1)*IPB + inode - 1);            /* Reference to the prev and    */
            iNode[inode].vD2.next = ((block-1)*IPB + inode + 1);            /* next inode in the double-    */
                                                /* linked list of free inodes   */
            for (i = 0; i < N_DIRECT; i++)      
                iNode[inode].d[i] = NULL_CLUSTER;           /* Put all direct and indirect references to    */
                                            /* data clusters pointing to null       */
            iNode[inode].i1 = iNode[inode].i2 = NULL_CLUSTER;              
        }                                  
 
        if((status = soStoreBlockInT()) != 0 )                  /* Store new values to storage device*/
            return status;
    }
 
    /* Fill in the inode 0 (root directory) */
 
    if((status = soLoadBlockInT(0)) != 0)           /* Load block's content to internal storage */
        return status;
 
    iNode = soGetBlockInT();                /* Get a pointer to the content */
 
    iNode[0].mode= INODE_RD_USR | INODE_WR_USR | INODE_EX_USR | INODE_RD_GRP | INODE_WR_GRP |
                   INODE_EX_GRP | INODE_RD_OTH | INODE_WR_OTH |INODE_EX_OTH | INODE_DIR;        /* flag signaling inode describes a directory and its permissions*/
    iNode[0].refcount=2;                        /* references to previous and own directory*/
    iNode[0].owner = getuid();
    iNode[0].group = getgid();
    iNode[0].size = CLUSTER_SIZE;               /* size of the first i-node is one cluster's size */
    iNode[0].clucount=1;
    iNode[0].vD1.atime=time(NULL);              /* time.h  */
    iNode[0].vD2.mtime=time(NULL);
    iNode[0].d[0] = 0;;                     /* first direct reference points to cluster 0*/
 
    iNode[1].vD1.prev = (((block-1)*IPB)-1);        /* The prev of first inode is the last inode of double-linked list  */
 
    if((status = soStoreBlockInT()) != 0 )          /* Store new values to storage device*/
        return status;
 
    if((status = soLoadBlockInT(p_sb->itable_size -1)) != 0) /* Load last block's content to internal storage */
        return status;
 
    iNode = soGetBlockInT();                    /* Get a pointer to the content */
    iNode[IPB-1].vD2.next = 1;                  /* The next of last inode, is first inode */
 
    if((status = soStoreBlockInT()) != 0)               /* Store new values to storage device*/
        return status;
     
    if((status = soStoreSuperBlock()) != 0)
        return status;
 
    return 0;
}

/*
 * filling in the contents of the root directory:
     the first 2 entries are filled in with "." and ".." references
     the other entries are empty
 */

static int fillInRootDir (SOSuperBlock *p_sb)
{
  SODirEntry root[DPC];                                         // array de DPC entradas de directório
  int i,j;                  
  int stat;                                                     // guarda o que a função soWriteCacheCluster retorna

  for (i = 0; i < DPC; i++)                                     // apagar as entradas do directorio
  {                                                             // colocando o 1º caracter do campo name como '\0'
    for(j = 0; j<=MAX_NAME; j++){
      root[i].name[j] = '\0';
    }
    root[i].nInode = NULL_INODE;                                // colocar o número do iNode a Null
  }

  // as duas 1ª's entradas de um directorio estão sempre ocupadas, por "." e ".."
  // para que a árvore de directorios possa ser implementada de um modo eficiente
  // Criar a entrada "." (auto-referencia)
  strcpy((char *)root[0].name, ".");                            // copia "." para o 1º caracter do name
  root[0].nInode = 0;

  // Criar a e entrada ".." (directorio acima na hierarquia)
  strcpy((char *)root[1].name, "..");                           // copia ".." para o 2º caracter do name
  root[1].nInode = 0;

  // write cache Cluster -> escreve um cluster de dados no buffercache, os argumentos são 
  // o número fisico do 1º bloco de dados do cluster a ser escrito, e um ponteiro para o buffer alocado anteriormente
  if((stat = soWriteCacheCluster(p_sb->dzone_start,root)) != 0) // dzone_start -> número fisico do 1º cluster de dados que vai ser escrito
    return stat;                                                // se retornar 0 não há problemas

  return 0;
}

  /*
   * create the table of references to free data clusters as a static circular FIFO
   * zero fill the remaining data clusters if full formating was required:
   *   zero mode was selected
   */

static int fillInTRefFDC (SOSuperBlock *p_sb, int zero)
{
    int status;                     //  error variable  
    int32_t i, j;           
    uint32_t* p_block;              //  pointer to superBlock type variable
    unsigned char cluster[BSLPC];   //  array of chars used to clear each block
 
    p_sb->tbfreeclust_tail = 0;     /* assign correct tail */
     
    for(i = 0; i < p_sb->tbfreeclust_size && (p_sb->tbfreeclust_tail < p_sb->dzone_total || zero) ; i++) 
    {
        // load internal memory block
        if((status = soLoadBlockFCT(i)) != 0) return status;
 
        // get block's reference
        p_block = soGetBlockFCT();
 
        // insert the references in the block 
        for (j = 0 ; j < RPB ; j++) 
        {
            if (p_sb->tbfreeclust_tail < p_sb->dzone_total) 
            {
                if((p_sb->tbfreeclust_tail==0)) 
                {
                    p_block[j]=NULL_CLUSTER;            /* first position is empty */
                    p_sb->tbfreeclust_tail++; 
                } 
                else
                    p_block[j] = p_sb->tbfreeclust_tail++;
            } 
            else p_block[j] = (uint32_t)(-2);
        }   
        // store the block in the disk
        if((status = soStoreBlockFCT()) != 0) return status;
    }
    // if zero erase data zone
    if(zero)
    {
 
        // Run all data zone, starting in the second data cluster
        for (j=p_sb->dzone_start+BLOCKS_PER_CLUSTER; j < p_sb->dzone_start+(p_sb->dzone_total*BLOCKS_PER_CLUSTER); j=j+BLOCKS_PER_CLUSTER)
        {
            // Writes zero in all cluster
            for (i = 0; i < BSLPC; i++)
                cluster[i]=0;
 
            // Cluster is updated
            if ((status = soWriteCacheCluster(j,cluster)) != 0)
                return status;
        }
 
    }
 
    // make sure that tail and head are in the correct positions
    p_sb->tbfreeclust_tail=0;
    p_sb->tbfreeclust_head=1;
     
    // store SuperBlock in the disk
    if((status = soStoreSuperBlock()) != 0) return status;
 
    return 0;
}

/*
 * check the consistency of the file system metadata
 */

static int checkFSConsist (void)
{
  SOSuperBlock *p_sb;                            /* pointer to the superblock */
  SOInode *inode;                                /* pointer to the contents of a block of the inode table */
  int stat;                                      /* status of operation */

  /* read the contents of the superblock to the internal storage area and get a pointer to it */
  if ((stat = soLoadSuperBlock ()) != 0) return stat;
  p_sb = soGetSuperBlock ();
  
  /* check superblock and related structures */
  if ((stat = soQCheckSuperBlock (p_sb)) != 0) return stat;

  /* read the contents of the first block of the inode table to the internal storage area and get a pointer to it */
  if ((stat = soLoadBlockInT (0)) != 0) return stat;
  inode = soGetBlockInT ();

  /* check inode associated with root directory (inode 0) and the contents of the root directory */

if ((stat = soQCheckInodeIU (p_sb, &inode[0])) != 0) return stat;
if ((stat = soQCheckDirCont (p_sb, &inode[0])) != 0) return stat;

  /* everything is consistent */

  return 0;
}
