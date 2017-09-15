#!/bin/bash

# This test vector deals with the operations alloc / free inodes.
# It defines a storage device with 19 blocks and formats it with an inode table of 8 inodes.
# It starts by allocating all the inodes and testing several error conditions. Then, it frees all the
# allocated inodes in the reverse order of allocation while still testing different error conditions.
# The showblock_sofs15 application should be used in the end to check metadata.

RUNDIR=../run

case "$1" in
    "-bin") mkfs=mkfs_sofs15_bin;;
    *) mkfs=mkfs_sofs15;;
esac

${RUNDIR}/createEmptyFile myDisk 19
${RUNDIR}/${mkfs} -n SOFS15 -i 8 -z myDisk
${RUNDIR}/testifuncs15 -b -l 600,700 -L testVector1.rst myDisk <testVector1.cmd

