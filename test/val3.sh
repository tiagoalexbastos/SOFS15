#!/bin/bash

# This test vector checks if a large pdf file can be copied to the root directory.
# Basic system calls involved: readdir, mknode, read and write.

RUNDIR=../run

echo -e '\n**** Creating the storage device.****\n'
${RUNDIR}/createEmptyFile myDisk 1000
echo -e '\n**** Converting the storage device into a SOFS15 file system.****\n'
${RUNDIR}/mkfs_sofs15 -i 56 -z myDisk
echo -e '\n**** Mounting the storage device as a SOFS15 file system.****\n'
${RUNDIR}/mount_sofs15 myDisk mnt
echo -e '\n**** Copying the text file.****\n'
cp "SOFS15.pdf" mnt
echo -e '\n**** Listing the root directory.****\n'
ls -la mnt
echo -e '\n**** Checking if the file was copied correctly.****\n'
diff "SOFS15.pdf" "mnt/SOFS15.pdf"
echo -e '\n**** Getting the file attributes.****\n'
stat mnt/"SOFS15.pdf"
echo -e '\n**** Unmounting the storage device.****\n'
sleep 1
fusermount -u mnt
