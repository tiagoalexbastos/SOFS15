#!/bin/bash

# This test vector deals mainly with operations alloc / free data clusters through the operation handle
# file cluster which is always used.
# It defines a storage device with 100 blocks and formats it with 23 data clusters.
# It starts by allocating an inode, then it proceeds by allocating 13 data clusters in all the reference
# areas (direct, single indirect and double indirect). This means that in fact 20 data clusters are
# allocated. Then all data clusters are freed in reverse order and the inode is also freed.
# The showblock_sofs15 application should be used in the end to check metadata.

RUNDIR=../run

case "$1" in
    "-bin") mkfs=mkfs_sofs15_bin;;
    *) mkfs=mkfs_sofs15;;
esac

${RUNDIR}/createEmptyFile myDisk 100
${RUNDIR}/${mkfs} -n SOFS15 -i 48 -z myDisk
${RUNDIR}/testifuncs15 -b -l 400,700 -L testVector7.rst myDisk <testVector7.cmd