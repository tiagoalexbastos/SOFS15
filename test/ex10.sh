#!/bin/bash

# This test vector deals mainly with operations get a directory entry by name, add / remove directory
# entries, rename directory entries and check a directory status of emptiness.
# It defines a storage device with 100 blocks and formats it with 48 inodes.
# It starts by allocating seven inodes, associated to regular files and directories, and organize them
# in a hierarchical faction. Then it proceeds by renaming some of the entries and ends by removing all
# of them.
# The showblock_sofs15 application should be used in the end to check metadata.

RUNDIR=../run

case "$1" in
    "-bin") mkfs=mkfs_sofs15_bin;;
    *) mkfs=mkfs_sofs15;;
esac

${RUNDIR}/createEmptyFile myDisk 100
${RUNDIR}/${mkfs} -n SOFS15 -i 48 -z myDisk
${RUNDIR}/testifuncs15 -b -l 300,700 -L testVector10.rst myDisk <testVector10.cmd
