/**

 *  \file soHandleFileCluster.c (implementation file)

 *

 *  \author ---

 */

#include <stdio.h>

#include <inttypes.h>

#include <errno.h>

#include "sofs_probe.h"

#include "sofs_buffercache.h"

#include "sofs_superblock.h"

#include "sofs_inode.h"

#include "sofs_datacluster.h"

#include "sofs_basicoper.h"

#include "sofs_basicconsist.h"

#include "sofs_ifuncs_1.h"

#include "sofs_ifuncs_2.h"

/** \brief operation get the logical number of the referenced data cluster */

#define GET         0

/** \brief operation  allocate a new data cluster and include it into the list of references of the inode which

 *                    describes the file */

#define ALLOC       1

/** \brief operation free the referenced data cluster and dissociate it from the inode which describes the file */

#define FREE        2

/* Allusion to internal functions */

int soHandleDirect (SOSuperBlock *p_sb, SOInode *p_inode, uint32_t nClust, uint32_t op, uint32_t *p_outVal);

int soHandleSIndirect (SOSuperBlock *p_sb, SOInode *p_inode, uint32_t nClust, uint32_t op, uint32_t *p_outVal);

int soHandleDIndirect (SOSuperBlock *p_sb, SOInode *p_inode, uint32_t nClust, uint32_t op, uint32_t *p_outVal);

/**

 *  \brief Handle of a file data cluster.

 *

 *  The file (a regular file, a directory or a symlink) is described by the inode it is associated to.

 *

 *  Several operations are available and can be applied to the file data cluster whose logical number is given.

 *

 *  The list of valid operations is

 *

 *    \li GET:        get the logical number (or reference) of the referred data cluster

 *    \li ALLOC:      allocate a new data cluster and associate it to the inode which describes the file

 *    \li FREE:       free the referred data cluster.

 *

 *  Depending on the operation, the field <em>clucount</em> and the lists of direct references, single indirect

 *  references and double indirect references to data clusters of the inode associated to the file are updated.

 *

 *  Thus, the inode must be in use and belong to one of the legal file types in all cases.

 *

 *  \param nInode number of the inode associated to the file

 *  \param clustInd index to the list of direct references belonging to the inode where the reference to the data cluster

 *                  is stored

 *  \param op operation to be performed (GET, ALLOC, FREE)

 *  \param p_outVal pointer to a location where the logical number of the data cluster is to be stored (GET / ALLOC);

 *                  in the other case (FREE) it is not used (it should be set to \c NULL)

 *

 *  \return <tt>0 (zero)</tt>, on success

 *  \return -\c EINVAL, if the <em>inode number</em> or the <em>index to the list of direct references</em> are out of

 *                      range or the requested operation is invalid or the <em>pointer to outVal</em> is \c NULL when it

 *                      should not be (GET / ALLOC)

 *  \return -\c EIUININVAL, if the inode in use is inconsistent

 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent

 *  \return -\c EDCARDYIL, if the referenced data cluster is already in the list of direct references (ALLOC)

 *  \return -\c EDCNOTIL, if the referenced data cluster is not in the list of direct references (FREE)

 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level

 *  \return -\c EBADF, if the device is not already opened

 *  \return -\c EIO, if it fails reading or writing

 *  \return -<em>other specific error</em> issued by \e lseek system call

 */

int soHandleFileCluster (uint32_t nInode, uint32_t clustInd, uint32_t op, uint32_t *p_outVal)
{
    //function that gives information about the operations about soHandleFileCluster
  soColorProbe (413, "07;31", "soHandleFileCluster (%"PRIu32", %"PRIu32", %"PRIu32", %p)\n",
                nInode, clustInd, op, p_outVal);

    SOSuperBlock *p_sb;     //pointer to the Super Block
    SOInode iNode;          //i-node data type
    uint32_t status;        //state variable used throughout the code -error

    /**********************Validation of Arguments***********************************/  
    //load of superBlock to internal storage
    if ((status = soLoadSuperBlock()) != 0)     return status;
    
    //Get a pointer to the contents of the superblock
    if ((p_sb = soGetSuperBlock()) == NULL) return -EIO;
    
    //checks if the number of the i-node is valid or out of range
    if ((nInode < 0) || (nInode > p_sb->itotal-1)) return -EINVAL;

    // validation of Options    
    if (op != GET && op != ALLOC && op != FREE) return -EINVAL;
    
    //validation of p_outval
    if(op == GET || op == ALLOC)
        if(p_outVal == NULL)
            return -EINVAL;

    //checks if File is of valid type   
    if ((status = soReadInode(&iNode,nInode)) != 0) return status;
        
    //checks if the index to the list of direct references is valid or out of range
    if ((clustInd<0) || (clustInd > (N_DIRECT + RPC + RPC*RPC)))
        return -EINVAL;
        
    //if op==FREE then p_outVal is set a NULL
    if (op == FREE) p_outVal = NULL;
    
    
    /**********************End of validation of Arguments******************************/    
    
    /******start :check of consistency******/
    //checks for the consistency of the table of inodes metadata        
    if ((status = soQCheckInT(p_sb)) != 0) return status;
    
    //checks for the consistency of the data zone metadata
    if ((status = soQCheckDZ(p_sb)) != 0) return status;
    /******end :check of consistency******/
    
    //depending on the clustInd there are: direct,single indirect or double indirect references
    //necessary internal funcion will be called to assist
    if (clustInd < N_DIRECT) 
    {   //it is a direct reference
        if ((status = soHandleDirect(p_sb,&iNode,clustInd,op,p_outVal)) != 0) 
            return status;
    }
    else if (clustInd < (N_DIRECT + RPC))
    {   //it is a single indirect reference
        if ((status = soHandleSIndirect(p_sb,&iNode,clustInd,op,p_outVal)) != 0)    
            return status;
    }
    else
    {   //it is a double indirect reference
        if ((status = soHandleDIndirect(p_sb,&iNode,clustInd,op,p_outVal)) != 0)    
            return status;
    }

    if (op!= GET){
    //writes the inode 
    if ((status = soWriteInode(&iNode,nInode)) != 0)
        return status;
    }
    //Stores the contents of the superblock into internal storage
    if ((status = soStoreSuperBlock()) != 0) return status;
    
    //SUCCESS
    return 0;
}

/**

 *  \brief Handle of a file data cluster whose reference belongs to the direct references list.

 *

 *  \param p_sb pointer to a buffer where the superblock data is stored

 *  \param p_inode pointer to a buffer which stores the inode contents

 *  \param clustInd index to the list of direct references belonging to the inode which is referred

 *  \param op operation to be performed (GET, ALLOC, FREE)

 *  \param p_outVal pointer to a location where the logical number of the data cluster is to be stored (GET / ALLOC);

 *                  in the other case (FREE) it is not used (it should be set to \c NULL)

 *

 *  \return <tt>0 (zero)</tt>, on success

 *  \return -\c EINVAL, if the requested operation is invalid

 *  \return -\c EDCARDYIL, if the referenced data cluster is already in the list of direct references (ALLOC)

 *  \return -\c EDCNOTIL, if the referenced data cluster is not in the list of direct references (FREE)

 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level

 *  \return -\c EBADF, if the device is not already opened

 *  \return -\c EIO, if it fails reading or writing

 *  \return -<em>other specific error</em> issued by \e lseek system call

 */

int soHandleDirect (SOSuperBlock *p_sb, SOInode *p_inode, uint32_t clustInd, uint32_t op, uint32_t *p_outVal)
{   
    //state variable used throughout the code
    uint32_t status;    

    /*Validation of Arguments*/
    if (p_sb == NULL || p_inode == NULL) return -EINVAL;
    
    switch(op)
    {
        case GET:
                *p_outVal = p_inode->d[clustInd];
            break;
            
        case ALLOC:
            //checks if the position on the table of direct references is empty 
            if (p_inode->d[clustInd] == NULL_CLUSTER)
            {   //Checks if there are  1 free dataCluster(necessary to allocate),if not -ENOSPC 
                if(p_sb->dzone_free <= 0)return -ENOSPC;    
                
                //allocs a data cluster
                if ((status = soAllocDataCluster(p_outVal)) != 0) return status;
                
                //inserts the logical number of the data cluster on the table of direct references
                p_inode->d[clustInd] = *p_outVal;
                
                
                p_inode->clucount++;
            }
            else 
                return -EDCARDYIL;
            
            break;
            
        case FREE:
            // position  is already free
            if (p_inode->d[clustInd] == NULL_CLUSTER) return -EDCNOTIL;
            
            
            if ((status = soFreeDataCluster(p_inode->d[clustInd])) != 0)
                    return status;
            //clean the position on the table of direct references 
            p_inode->d[clustInd] = NULL_CLUSTER;
            
            
            p_inode->clucount--;
            
            break;
            
        default:
            return -EINVAL; 
            
            break;
    }
   
    return 0;
}

/**

 *  \brief Handle of a file data cluster which belongs to the single indirect references list.

 *

 *  \param p_sb pointer to a buffer where the superblock data is stored

 *  \param p_inode pointer to a buffer which stores the inode contents

 *  \param clustInd index to the list of direct references belonging to the inode which is referred

 *  \param op operation to be performed (GET, ALLOC, FREE)

 *  \param p_outVal pointer to a location where the logical number of the data cluster is to be stored (GET / ALLOC);

 *                  in the other case (FREE) it is not used (it should be set to \c NULL)

 *

 *  \return <tt>0 (zero)</tt>, on success

 *  \return -\c EINVAL, if the requested operation is invalid

 *  \return -\c EDCARDYIL, if the referenced data cluster is already in the list of direct references (ALLOC)

 *  \return -\c EDCNOTIL, if the referenced data cluster is not in the list of direct references (FREE)

 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level

 *  \return -\c EBADF, if the device is not already opened

 *  \return -\c EIO, if it fails reading or writing

 *  \return -<em>other specific error</em> issued by \e lseek system call

 */

int soHandleSIndirect (SOSuperBlock *p_sb, SOInode *p_inode, uint32_t clustInd, uint32_t op, uint32_t *p_outVal)
{   
    uint32_t status;    //state variable used throughout the code
    uint32_t i;         // variable used in for's
    
    // indice of cluster in cluster of direct references (clustInd - N_DIRECT)
    uint32_t indDInd = clustInd - N_DIRECT;
    // pointer to cluster of direct references
    SODataClust *p_dc;

    /***start of validation of Arguments***/
    if (p_sb == NULL || p_inode == NULL)
        return -EINVAL;
    /***end of validation of Arguments***/
    
    switch(op)
    {
        case GET:
            // if cluster of direct references doesnt exist
            if (p_inode->i1 == NULL_CLUSTER)
                *p_outVal = NULL_CLUSTER;
            else
            {// Load content of cluster of direct references
                if ((status = soLoadDirRefClust( p_inode -> i1 * BLOCKS_PER_CLUSTER + (p_sb->dzone_start))) != 0 )
                    return status;
                // p_dc is the pointer for that cluster
                if ((p_dc = soGetDirRefClust()) == NULL ) return -EIO;
                
                // return the number of cluster in indDInd position of the array (cluster) of direct references
                *p_outVal = p_dc->ref[indDInd];
            }
            break;
        case ALLOC:
            // if cluster of direct references doesnt exist
            if (p_inode->i1 == NULL_CLUSTER)
            {   
                /**Checks if there are  2 free dataCluster(necessary to allocate),if not -ENOSPC **/
                if(p_sb->dzone_free <= 1)return -ENOSPC;    
                    
                // alloc of cluster to serve as array of direct references
                if ((status = soAllocDataCluster(p_outVal)) != 0)
                    return status;
                    
                // puts in no-i the reference to cluster of direct references
                p_inode->i1 = *p_outVal;
                
                // increments the number of clusters associated to no-i
                p_inode->clucount++;
                
                // loads the content of cluster of direct references
                if ((status = soLoadDirRefClust(p_sb->dzone_start + BLOCKS_PER_CLUSTER * p_inode->i1)) != 0)
                    return status;
                    
                // p_dc is the pointer for that cluster
                if ((p_dc = soGetDirRefClust()) == NULL)  return -EIO;
                
                // initialize the array of direct references to NULL_CLUSTER
                for (i = 0;i<RPC;i++)
                    p_dc->ref[i] = NULL_CLUSTER;
                
                // store of cluster of direct references    
                if ((status = soStoreDirRefClust()) != 0)
                    return status;
                    
                // alloc data cluster in use    
                if ((status = soAllocDataCluster(p_outVal)) != 0)  return status;
                
                // loads the content of direct references cluster
                // because anterior function uses memory that contains the direct references cluster
                if ((status = soLoadDirRefClust(p_sb->dzone_start + BLOCKS_PER_CLUSTER * p_inode->i1)) != 0)
                    return status;
                    
                // p_dc is the pointer for that cluster
                if ((p_dc = soGetDirRefClust()) == NULL)  return -EIO;
                
                // puts the allocated cluster in the array of direct references
                p_dc->ref[indDInd] = *p_outVal;
                
                // increments the number of clusters associated to no-i
                p_inode->clucount++;
                
                // store of cluster of direct references    
                if ((status = soStoreDirRefClust()) != 0)
                    return status;
                    
                
            }
            else
            {   /**Checks if there are  1 free dataCluster(necessary to allocate),if not -ENOSPC **/
                    if(p_sb->dzone_free <= 0)return -ENOSPC;    
                    
                    // loads the content of direct references cluster
                if ((status = soLoadDirRefClust(p_sb->dzone_start + BLOCKS_PER_CLUSTER * p_inode->i1)) != 0)
                    return status;
                // p_dc is the pointer for that cluster
                if ((p_dc = soGetDirRefClust()) == NULL)
                    return -EIO;
                // Checks if there is a cluster
                if (p_dc->ref[indDInd] == NULL_CLUSTER)                     
                {
                    // if not allocate one
                    if ((status = soAllocDataCluster(p_outVal)) != 0)  return status;
                
                    // puts the allocated cluster in the array of direct references
                    p_dc->ref[indDInd] = *p_outVal;
                    
                    // increments the number of clusters associated to no-i
                    p_inode->clucount++;
                
                    // store of cluster of direct references    
                    if ((status = soStoreDirRefClust()) != 0)  return status;
                }
                else
                    return -EDCARDYIL;
            }
            break;
            
        case FREE:
        // if cluster of direct references exist's
            if (p_inode->i1 != NULL_CLUSTER)
            {   // loads the content of direct references cluster
                if ((status = soLoadDirRefClust( p_inode -> i1 * BLOCKS_PER_CLUSTER + (p_sb->dzone_start))) != 0 )
                    return status;
                // p_dc is the pointer for that cluster
                if ((p_dc = soGetDirRefClust()) == NULL)  return -EIO;
                // Checks if cluster in pretended position is NULL_CLUSTER
                if ( p_dc->ref[indDInd] == NULL_CLUSTER )  return -EDCNOTIL;
                
                // free the cluster
                if ((status = soFreeDataCluster( p_dc->ref[indDInd] )) != 0)  return status;
                        
                // Remove the association of the cluster to the no-i
                //(clears position in cluster of direct references)     
                p_dc->ref[indDInd] = NULL_CLUSTER;  
                    
                // decrements number of clusters associated to no-i         
                p_inode -> clucount--;  
                    
                // store cluster of direct references
                if ((status = soStoreDirRefClust()) != 0)  return status;
                    
                /** checks the i1 fiels of no-i if there are references left,
                 *  if not, clean and remove cluster.   ***/
                // checks for references
                for (i = 0; i < RPC; i++)
                {
                    if ( (p_dc->ref[i]) != NULL_CLUSTER )
                        break;                  
                }   // if not,clean and remove cluster  
                    if (i == RPC)
                    {   // free cluster
                        if ((status = soFreeDataCluster(p_inode->i1)) != 0)
                            return status;
                            //  i1 is set to NULL_CLUSTER
                        p_inode->i1 = NULL_CLUSTER;
                        // decrements number of clusters associated to no-i     
                        p_inode->clucount--;
                    }                   
            }
            else
                return -EDCNOTIL;// cannot be  FREE, there are no cluster in array of direct references
                                    
            break;
            
        default:
            return -EINVAL;  //op is not valid
            break;
    }
    //SUCCESS
  return 0;
}

/**
 *  \brief Handle of a file data cluster which belongs to the double indirect references list.
 *
 *  \param p_sb pointer to a buffer where the superblock data is stored
 *  \param p_inode pointer to a buffer which stores the inode contents
 *  \param clustInd index to the list of direct references belonging to the inode which is referred
 *  \param op operation to be performed (GET, ALLOC, FREE)
 *  \param p_outVal pointer to a location where the logical number of the data cluster is to be stored (GET / ALLOC);
 *                  in the other case (FREE) it is not used (it should be set to \c NULL)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the requested operation is invalid
 *  \return -\c EDCARDYIL, if the referenced data cluster is already in the list of direct references (ALLOC)
 *  \return -\c EDCNOTIL, if the referenced data cluster is not in the list of direct references (FREE)
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soHandleDIndirect (SOSuperBlock *p_sb, SOInode *p_inode, uint32_t clustInd, uint32_t op, uint32_t *p_outVal)
{   
    uint32_t status;        //state variable used throughout the code
    uint32_t i;             //variable to use in the for
    uint32_t i1;    //saves the value of the cluster of the single indirect references
    
    //relative position inside the table of single indirect references
    uint32_t indDInd = (clustInd-N_DIRECT-RPC)/RPC; 
    //relative position inside the table of direct references
    uint32_t indSInd = (clustInd-N_DIRECT-RPC)%RPC;
    
    // pointer to a cluster of double indirect references,
    //single indirect references and direct references
    SODataClust *p_dInd,*p_sInd,*p_dc;

    /***start of validation of Arguments***/
    if (p_sb == NULL || p_inode == NULL)
        return -EINVAL;
    /***end of validation of Arguments***/
    
    switch(op)
    {
        case GET:
        // //the table of double indirect it does not exist
            if (p_inode->i2 == NULL_CLUSTER)
                *p_outVal = NULL_CLUSTER;  //there isn't any cluster in the position
            else
            {   //gets the table of double indirect and reference
                if((status=soLoadSngIndRefClust(p_inode->i2* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                    return status;
                p_dInd = soGetSngIndRefClust();
                
                //saves the value of the position of the table of indirect references to a temporary variable
                i1 = p_dInd->ref[indDInd];
                
                //the table of single indirect doen't exist
                if(i1==NULL_CLUSTER)
                    *p_outVal = NULL_CLUSTER;
                else
                {   //loads and get reference of single indirect table
                    if((status=soLoadDirRefClust(i1* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                        return status;

                    if ((p_sInd = soGetDirRefClust()) == NULL)  return -EIO;
                    //the table of single indirect exist    
                    *p_outVal = p_sInd->ref[indSInd];
                }
            }
            break;
        case ALLOC:
            //the table of double indirect is does not exist
            if (p_inode->i2 == NULL_CLUSTER)
            {
                /**Checks if there are  3 free dataCluster(necessary to allocate),if not -ENOSPC **/
                    if(p_sb->dzone_free <= 2)
                        return -ENOSPC; 
                    
                /**allocs a data cluster to create a table of indirect references to i2 
                    and sets the values in the table to NULL_CLUSTER**/
                //start
                if ((status = soAllocDataCluster(p_outVal)) != 0) return status;                        
                
                    p_inode->i2=*p_outVal;
                //increments the total number of data clusters attached to the file because 
                //the cluster of double indirect references also counts
                    p_inode->clucount++;
                    
                if ((status = soLoadSngIndRefClust(p_sb->dzone_start + BLOCKS_PER_CLUSTER * p_inode->i2)) != 0)
                    return status;
                    
                if ((p_dc = soGetSngIndRefClust()) == NULL)  return -EIO;
                    
                for (i = 0;i<RPC;i++)
                    p_dc->ref[i] = NULL_CLUSTER;
                    
                if ((status = soStoreSngIndRefClust()) != 0)  return status;    
                //end
                    
                /**allocs a data cluster to create a table of direct references
                *puts the value of the allocated data cluster on the positon ind_DInd 
                * of the table of double indirect references**/
                //start
                if((status=soAllocDataCluster(&i1))!=0)                             
                {   if(status == -ENOSPC )
                    return status;
                }
                
                //increments the total number of data clusters attached to the file 
                //because the cluster of single indirect references also counts
                p_inode->clucount++;
                
                if((status=soLoadSngIndRefClust(p_inode->i2* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                    return status;
                if ((p_dInd = soGetSngIndRefClust()) == NULL)
                    return -EIO;
                    
                p_dInd->ref[indDInd] = i1;

                if((status=soStoreSngIndRefClust())!=0)
                    return status;
                //end
                
                /**sets the value in the table of direct references to NULL_CLUSTER     */
                //start
                if((status=soLoadDirRefClust(i1* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                    return status;
                if ((p_sInd = soGetDirRefClust()) == NULL)
                    return -EIO;
                
                for (i=0; i<RPC; i++)
                    p_sInd->ref[i] = NULL_CLUSTER;

                if((status=soStoreDirRefClust())!=0)
                    return status;
                //end
                
                
                /**alloc a data cluster to store on the table of direct references**/               
                //start
                if((status=soAllocDataCluster(p_outVal))!=0)
                    return status;
                //increments the total number of data clusters attached to the file
                p_inode -> clucount++;

                if((status=soLoadSngIndRefClust(p_inode->i2* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                    return status;

                if ((p_dInd = soGetSngIndRefClust()) == NULL)
                    return -EIO;
            
                if((status=soLoadDirRefClust(i1* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                    return status;

                if ((p_sInd = soGetDirRefClust()) == NULL)
                    return -EIO;
                
                p_sInd->ref[indSInd] = *p_outVal;

                if((status=soStoreDirRefClust())!=0)
                    return status;

                if((status=soStoreSngIndRefClust())!=0)
                    return status;
                //end   
            }
            else
            {   /**Checks if there are  2 free dataCluster(necessary to allocate),if not -ENOSPC **/
                    if(p_sb->dzone_free <= 1 )return -ENOSPC;
                
                //gets the table of double indirect and reference
                if((status=soLoadSngIndRefClust(p_inode->i2* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                    return status;

                if ((p_dInd = soGetSngIndRefClust()) == NULL)
                    return -EIO;
                //saves the value of the position of the table of indirect references to a temporary variable
                i1=p_dInd->ref[indDInd];
                
                if (i1 == NULL_CLUSTER)
                {   //stores the table os double ind because we going to use the AllocDataCluster
                    if((status=soStoreSngIndRefClust())!=0)  return status;
                    
                    if((status=soAllocDataCluster(p_outVal))!=0) return status;                     
                    i1= *p_outVal;
                    //increments the total number of data clusters attached to the file 
                    //because the cluster of single indirect references also counts
                    p_inode -> clucount++;
                    
                    /**puts the value of the allocated data cluster on the positon 
                       ind_refDInd of the table of double indirect references**/
                      //start
                    if((status=soLoadSngIndRefClust(p_inode->i2* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                        return status;

                    if ((p_dInd = soGetSngIndRefClust()) == NULL)  return -EIO;
        
                    p_dInd->ref[indDInd] = i1;

                    if((status=soStoreSngIndRefClust())!=0)  return status;
                    //end
                    
                    /**sets the values of direct references in the table to NULL_CLUSTER**/
                    //start
                    if((status=soLoadDirRefClust(i1* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                        return status;

                    if ((p_sInd = soGetDirRefClust()) == NULL)
                        return -EIO;
                    
                    for (i=0; i<RPC; i++)
                        p_sInd->ref[i] = NULL_CLUSTER;

                    if((status=soStoreDirRefClust())!=0)
                        return status;
                    //end
                    
                    /**alloc a data cluster to store on the table of direct references**/           
                    //start
                    if((status=soAllocDataCluster(p_outVal))!=0)  return status;
                        
                    //increments the total number of data clusters attached to the file
                    p_inode -> clucount++;

                    if((status=soLoadSngIndRefClust(p_inode->i2* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                        return status;

                    if ((p_dInd = soGetSngIndRefClust()) == NULL)  return -EIO;

                    if((status=soLoadDirRefClust(i1* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                        return status;

                    if ((p_sInd = soGetDirRefClust()) == NULL)  return -EIO;
                
                    p_sInd->ref[indSInd] = *p_outVal;

                    if((status=soStoreDirRefClust())!=0)  return status;
                    if((status=soStoreSngIndRefClust())!=0)  return status;
                    //end
                }
                else //i1 != NULL_CLUSTER
                {   // Load content of cluster of direct references
                    if((status=soLoadDirRefClust(i1* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                        return status;
                        
                    if ((p_sInd = soGetDirRefClust()) == NULL) return -EIO;
                    
                    //checks if the position isn't free
                    if (p_sInd->ref[indSInd] != NULL_CLUSTER )  return -EDCARDYIL;
                    
                    //stores the table of double & single because we going to use the AllocDataCluster
                    if((status=soStoreDirRefClust())!=0)  return status;
                    if((status=soStoreSngIndRefClust())!=0)  return status;
                    
                    //alloc a data cluster to store on the table of direct references
                    if((status=soAllocDataCluster(p_outVal))!=0)  return status;    
                    
                    //increments the total number of data clusters attached to the 
                    //file because the cluster of single indirect references also counts        
                    p_inode -> clucount++;
                    
                    /**fill the position in the table of direct ref with the logical number 
                     * of the cluster allocated */
                     //start
                    if((status=soLoadSngIndRefClust(p_inode->i2* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                        return status;
                    if ((p_dInd = soGetSngIndRefClust()) == NULL)
                        return -EIO;    
                    if((status=soLoadDirRefClust(i1* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                        return status;
                    if ((p_sInd = soGetDirRefClust()) == NULL)
                        return -EIO;

                    p_sInd->ref[indSInd] = *p_outVal;

                    if((status=soStoreDirRefClust())!=0)  return status;

                    if((status=soStoreSngIndRefClust())!=0) return status;
                }   //end
            }
            break;

        case FREE:
            
            if(p_inode->i2 != NULL_CLUSTER)
            {   
                if((status=soLoadSngIndRefClust(p_inode->i2* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                    return status;
                if ((p_dInd = soGetSngIndRefClust()) == NULL)
                    return -EIO;
                //saves the value of the position of the table of indirect references to a temporary variable
                i1=p_dInd->ref[indDInd];
                
                if (i1 != NULL_CLUSTER)
                {
                    if((status=soLoadDirRefClust(i1* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                        return status;
                    if ((p_sInd = soGetDirRefClust()) == NULL)
                        return -EIO;
                    //checks if the position is not free yet 
                    if (p_sInd->ref[indSInd] == NULL_CLUSTER )  return -EDCNOTIL;   
                    //free the data cluster
                    if((status=soFreeDataCluster(p_sInd->ref[indSInd]))!=0)
                        return status;
                    
                    // Remove the association of the cluster to the no-i    
                    p_sInd->ref[indSInd]=NULL_CLUSTER;
                    
                    //decrements the total number of data clusters attached to the file
                    p_inode->clucount--;

                    if((status=soStoreDirRefClust())!=0)
                        return status;
                    
                    //loads again the table of direct references
                    if((status=soLoadDirRefClust(i1* BLOCKS_PER_CLUSTER + (p_sb->dzone_start)))!=0)
                        return status;

                    if ((p_sInd = soGetDirRefClust()) == NULL)
                        return -EIO;
                    /**CHECKS IF THERE ARE ANY REFERENCES IN i1, if not it has to be freed**/
                    //start 
                    for (i = 0; i < RPC; i++)
                    {
                        if (p_sInd-> ref[i] != NULL_CLUSTER )
                            break;
                    }

                    if (i == RPC)
                    {
                        if((status=soFreeDataCluster(p_dInd -> ref[indDInd]))!=0)
                            return status;
            
                        p_dInd->ref[indDInd] = NULL_CLUSTER;
                        p_inode -> clucount--;      
                    }
                    
                    if((status=soStoreSngIndRefClust())!=0)  return status;
                    //end
                    
                    /******* CHECKS IF THERE ARE ANY REFERENCES IN i2, if not it has to be freed  **/
                    //start
                    if ((status = soLoadSngIndRefClust( p_inode -> i2  * BLOCKS_PER_CLUSTER + (p_sb->dzone_start))) != 0)
                        return status;

                    if ((p_dInd = soGetSngIndRefClust()) == NULL)
                        return -EIO;

                    for (i = 0; i < RPC; i++)
                    {
                        if (p_dInd-> ref[i] != NULL_CLUSTER )
                            break;  
                    }

                    if (i == RPC)
                    {
                        if((status=soFreeDataCluster(p_inode -> i2))!=0)
                            return status;

                        p_inode -> i2 = NULL_CLUSTER;
                        p_inode -> clucount--;
                    }
                    //end
                }
                else//can't do this operation because there isn't any cluster
                    return -EDCNOTIL;
            }//can't do this operation because there isn't any cluster
            else
                return -EDCNOTIL;
            break;
        default:
            return -EINVAL; //invalid op
            break;
    }
    //SUCCESS
  return 0;
}