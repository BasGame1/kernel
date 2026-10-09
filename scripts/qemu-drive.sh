#!/bin/bash

DISK=disk.img
QEMU=qemu-system-riscv32
ARGS="-machine virt -bios none -kernel out/kernel -nographic"
DRIVE_ARGS="-drive file=$DISK,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0"

$QEMU $ARGS $DRIVE_ARGS
