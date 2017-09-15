/**
 *  \file soReaddir.c (implementation file)
 *
 *  \author Joana Conde 35818
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
 *  \brief Read a directory entry from a directory.
 *
 *  It tries to emulate <em>getdents</em> system call, but it reads a single directory entry in use at a time.
 *
 *  Only the field <em>name</em> is read.
 *
 *  \remark The returned value is the number of bytes read from the directory in order to get the next in use
 *          directory entry. So, skipped free directory entries must be accounted for. The point is that the system
 *          (through FUSE) uses the returned value to update file position.
 *
 *  \param ePath path to the file
 *  \param buff pointer to the buffer where data to be read is to be stored
 *  \param pos starting [byte] position in the file data continuum where data is to be read from
 *
 *  \return <em>number of bytes effectively read to get a directory entry in use (0, if the end is reached)</em>,
 *          on success
 *  \return -\c EINVAL, if either of the pointers are \c NULL or or the path string is a \c NULL string or the path does
 *                      not describe an absolute path or <em>pos</em> value is not a multiple of the size of a
 *                      <em>directory entry</em>
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ERELPATH, if the path is relative
 *  \return -\c EFBIG, if the <em>pos</em> value is beyond the maximum file size
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt> is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EPERM, if the process that calls the operation has not read permission on the directory described by
 *                     <tt>ePath</tt>
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soReaddir (const char *ePath, void *buff, int32_t pos)
{
  	soColorProbe (234, "07;31", "soReaddir (\"%s\", %p, %u)\n", ePath, buff, pos);

  	/* Validate entry arguments */
  	if((buff == NULL) || (ePath == NULL)) return -EINVAL;

 	uint32_t	clustIndex, offset, nClusters, readBytes, searchIndex, nInode;
	SOInode		Inode;
	SODirEntry	InodeDir[DPC];
	int			status,i,j;
	
	/* check path and if it is valid, return the inode */
	if((status = soGetDirEntryByPath(ePath, NULL, &nInode)) != 0) return status;

	/* read the inode */
	if((status = soReadInode( &Inode, nInode)) != 0) return status;

	/* check if it is a directory */
	if((Inode.mode & INODE_DIR) != INODE_DIR ) return -ENOTDIR;

	/* check if we have permissions to read dir */
	if((status = soAccessGranted( nInode, R )) != 0) return -EPERM;
		
	/* calculate the clusters number of direct references */
	if(Inode.clucount > 0 ) nClusters = Inode.clucount;
	
	/* clear i1 reference */
	if((nClusters > N_DIRECT) && (nClusters < (RPC + N_DIRECT))) nClusters--;	// i1 reference
	
	/* clear i2 references */
	if(nClusters > (RPC+N_DIRECT) )
		nClusters -= ((nClusters -1 -1 - N_DIRECT - RPC) + (nClusters -1 -1 -N_DIRECT - RPC)% (RPC+1)) / (RPC+1) +1;
	
	clustIndex =  pos / (sizeof(SODirEntry) * DPC);
	offset = pos / sizeof(SODirEntry);

	searchIndex = offset;
	readBytes = 0;

	for(i = clustIndex; i <  nClusters; i++)
	{
		if (i == clustIndex + 1) searchIndex = 0;
		
		/* read cluster from index */
		if((status = soReadFileCluster(nInode, i, &InodeDir)) !=0) return status;

		for(j = searchIndex; j < DPC; j++)
		{
			if(InodeDir[j].name[0] != '\0')
			{
				strcpy( (char *) buff, (char *) InodeDir[j].name);
				readBytes += sizeof(SODirEntry);			 	
				return readBytes;
			}
			else
				readBytes += sizeof(SODirEntry);				
		}
	}
	
	return 0;
}
