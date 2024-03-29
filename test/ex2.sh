#!/bin/bash

# This test vector deals with the operations alloc / free inodes.
# It defines a storage device with 21 blocks and formats it with an inode table of 24 inodes.
# It starts by allocating successive inodes until there are no more inodes. Then, it frees all
# the allocated inodes in a sequence where an inode stored in a different block of the table is
# taken in succession. Finally, it procedes to allocate two inodes.
# The showblock_sofs15 application should be used in the end to check metadata.


RUNDIR=../run

case "$1" in
    "-bin") mkfs=mkfs_sofs15_bin;;
    *) mkfs=mkfs_sofs15;;
esac

${RUNDIR}/createEmptyFile myDisk 21
${RUNDIR}/${mkfs} -n SOFS15 -i 24 -z myDisk
${RUNDIR}/testifuncs15 -b -l 600,700 -L testVector2.rst myDisk <testVector2.cmd

