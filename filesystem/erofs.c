/*
 * Ported from the Linux kernel (fs/erofs/erofs_fs.h, super.c)
 * Copyright (C) Linus Torvalds, Gao Xiang, and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/gid.h"
#include "../include/drivers.h"

#define EROFS_SUPER_MAGIC_V1    0xE0F5E1E2
#define EROFS_SUPER_OFFSET      1024

struct erofs_super_block {
   uint32_t magic;
   uint32_t checksum;
   uint32_t feature_compat;
   uint8_t  blkszbits;
   uint8_t  sb_extslots;
   uint16_t root_nid;
   uint64_t inos;
   uint64_t build_time;
   uint32_t build_time_nsec;
   uint32_t blocks;
   uint32_t meta_blkaddr;
   uint32_t xattr_blkaddr;
   uint8_t  uuid[16];
   uint8_t  volume_name[16];
   uint32_t feature_incompat;
   uint16_t extra_devices;
   uint16_t devt_slotoff;
   uint8_t  reserved[38];
} __attribute__((packed));

static bool erofs_mounted = false;
static uint32_t erofs_block_size = 4096;

extern int virtio_read_sector(uint64_t sector, void *buffer);

void init_erofs(void) {
   static uint8_t sb_buf[512];
   infon("Probing EROFS (Enhanced Read-Only File System) at offset 1024");
   /* Superblock at byte 1024 = sector 2 */
   if (virtio_read_sector(2, sb_buf) != 0) {
      errorn("erofs: failed reading superblock sector 2");
      return;
   }// if

   struct erofs_super_block *sb = (struct erofs_super_block *)sb_buf;
   if (sb->magic == EROFS_SUPER_MAGIC_V1) {
      erofs_block_size = 1 << sb->blkszbits;
      erofs_mounted = true;
      successn("erofs: valid high-performance read-only filesystem mounted");
   } else {
      errorn("erofs: magic signature mismatch");
   }// if
}// init_erofs

bool erofs_is_mounted(void) {
   return erofs_mounted;
}// erofs_is_mounted
