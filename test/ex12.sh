#!/bin/bash

# This test vector deals mainly with the operation get a directory entry by path.
# It defines a storage device with 100 blocks and formats it with 48 inodes.
# It starts by allocating seven inodes, associated to regular files and directories, and organize them
# in a hierarchical faction. Then it proceeds by trying to find different directory entries through the
# use of different paths.
# The showblock_sofs15 application should be used in the end to check metadata.

RUNDIR=../run

case "$1" in
    "-bin") mkfs=mkfs_sofs15_bin;;
    *) mkfs=mkfs_sofs15;;
esac

${RUNDIR}/createEmptyFile myDisk 100
${RUNDIR}/${mkfs} -n SOFS15 -i 48 -z myDisk
${RUNDIR}/testifuncs15 -b -l 300,700 -L testVector12.rst myDisk <testVector12.cmd
