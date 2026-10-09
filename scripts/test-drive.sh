#!/bin/bash

ROOT_DIR=$(pwd)
DISK=disk.img
TMP_PATH=$ROOT_DIR/tmp/mount/ext2-test

dd if=/dev/zero of=$DISK bs=1M count=32
mkfs.ext2 -F -b 1024 disk.img
mkdir -p $TMP_PATH
sudo mount $DISK $TMP_PATH
sudo touch $TMP_PATH/test
sudo umount $TMP_PATH
rm -rf $TMP_PATH
