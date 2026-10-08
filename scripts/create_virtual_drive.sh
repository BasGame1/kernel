#!/bin/bash

ROOT_DIR=$(pwd)
DISK=virtual_disk.img
DEFAULT_SIZE=500M

if [ -z $1 ]; then
   SIZE=$DEFAULT_SIZE
else
   SIZE=$1
fi

if [ -f $DISK ]; then
   if [ -f $DISK.bak ]; then
      rm -rf $DISK.bak
   fi
   mv $DISK $DISK.bak
fi

mke2fs -t ext2 $DISK $SIZE
