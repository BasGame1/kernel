/*
 * Ported from the Linux kernel (fs/btrfs/ctree.h, super.c)
 * Copyright (C) Linus Torvalds and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/gid.h"
#include "../include/drivers.h"

#define BTRFS_MAGIC 0x4D5F53665248425FULL

struct btrfs_header {
   uint8_t  csum[32];
   uint8_t  fsid[16];
   uint64_t bytenr;
   uint64_t flags;
   uint8_t  chunk_tree_uuid[16];
   uint64_t generation;
   uint64_t owner;
   uint32_t nritems;
   uint8_t  level;
} __attribute__((packed));

struct btrfs_super_block {
   uint8_t  csum[32];
   uint8_t  fsid[16];
   uint64_t bytenr;
   uint64_t flags;
   uint64_t magic;
   uint64_t generation;
   uint64_t root;
   uint64_t chunk_root;
   uint64_t log_root;
   uint64_t total_bytes;
   uint64_t bytes_used;
   uint64_t root_dir_objectid;
   uint64_t num_devices;
   uint32_t sectorsize;
   uint32_t nodesize;
   uint32_t leafsize;
   uint32_t stripesize;
} __attribute__((packed));

static bool btrfs_mounted = false;
static uint32_t btrfs_sectorsize = 4096;

extern int virtio_read_sector(uint64_t sector, void *buffer);

void init_btrfs(void) {
   static uint8_t sb_buf[1024];
   infon("Probing Btrfs filesystem (superblock offset 64KB)");
   /* Superblock at 64KB = sector 128 */
   if (virtio_read_sector(128, sb_buf) != 0 || virtio_read_sector(129, sb_buf + 512) != 0) {
      errorn("btrfs: failed reading primary superblock");
      return;
   }// if

   struct btrfs_super_block *sb = (struct btrfs_super_block *)sb_buf;
   if (sb->magic == BTRFS_MAGIC) {
      btrfs_sectorsize = sb->sectorsize ? sb->sectorsize : 4096;
      btrfs_mounted = true;
      successn("btrfs: valid B-tree filesystem superblock detected, mounted");
   } else {
      errorn("btrfs: magic signature mismatch");
   }// if
}// init_btrfs

bool btrfs_is_mounted(void) {
   return btrfs_mounted;
}// btrfs_is_mounted
