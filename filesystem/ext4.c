/*
 * Ported from the Linux kernel (fs/ext4/ext4.h, fs/ext4/super.c, fs/ext4/extents.c)
 * Copyright (C) Linus Torvalds and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/drivers.h"

#define EXT4_SUPER_MAGIC 0xEF53
#define EXT4_EXTENTS_MAGIC 0xF30A

struct ext4_super_block {
   uint32_t s_inodes_count;
   uint32_t s_blocks_count_lo;
   uint32_t s_r_blocks_count_lo;
   uint32_t s_free_blocks_count_lo;
   uint32_t s_free_inodes_count;
   uint32_t s_first_data_block;
   uint32_t s_log_block_size;
   uint32_t s_log_cluster_size;
   uint32_t s_blocks_per_group;
   uint32_t s_clusters_per_group;
   uint32_t s_inodes_per_group;
   uint32_t s_mtime;
   uint32_t s_wtime;
   uint16_t s_mnt_count;
   uint16_t s_max_mnt_count;
   uint16_t s_magic;
   uint16_t s_state;
   uint16_t s_errors;
   uint16_t s_minor_rev_level;
   uint32_t s_lastcheck;
   uint32_t s_checkinterval;
   uint32_t s_creator_os;
   uint32_t s_rev_level;
   uint16_t s_def_resuid;
   uint16_t s_def_resgid;
   uint32_t s_first_ino;
   uint16_t s_inode_size;
   uint16_t s_block_group_nr;
   uint32_t s_feature_compat;
   uint32_t s_feature_incompat;
   uint32_t s_feature_ro_compat;
   uint8_t  s_uuid[16];
   char     s_volume_name[16];
};

struct ext4_extent_header {
   uint16_t eh_magic;
   uint16_t eh_entries;
   uint16_t eh_max;
   uint16_t eh_depth;
   uint32_t eh_generation;
};

struct ext4_extent {
   uint32_t ee_block;
   uint16_t ee_len;
   uint16_t ee_start_hi;
   uint32_t ee_start_lo;
};

struct ext4_extent_idx {
   uint32_t ei_block;
   uint32_t ei_leaf_lo;
   uint16_t ei_leaf_hi;
   uint16_t ei_unused;
};

struct ext4_inode {
   uint16_t i_mode;
   uint16_t i_uid;
   uint32_t i_size_lo;
   uint32_t i_atime;
   uint32_t i_ctime;
   uint32_t i_mtime;
   uint32_t i_dtime;
   uint16_t i_gid;
   uint16_t i_links_count;
   uint32_t i_blocks_lo;
   uint32_t i_flags;
   uint32_t i_osd1;
   uint32_t i_block[15];
   uint32_t i_generation;
   uint32_t i_file_acl_lo;
   uint32_t i_size_high;
   uint32_t i_obso_faddr;
};

static uint32_t ext4_block_size = 1024;
static uint32_t ext4_inode_size = 256;
static bool ext4_initialized = false;

extern int virtio_read_sector(uint64_t sector, void *buffer);

int ext4_read_block(uint32_t block, void *buffer) {
   uint32_t sectors_per_block = ext4_block_size / 512;
   uint32_t start_sector = block * sectors_per_block;
   for (uint32_t i = 0; i < sectors_per_block; i++) {
      if (virtio_read_sector(start_sector + i, (uint8_t *)buffer + (i * 512)) != 0) {
         errorn("ext4: sector read error");
         return -1;
      }// if
   }// for
   return 0;
}// ext4_read_block

bool ext4_check_magic(const void *sb_buf) {
   const struct ext4_super_block *sb = (const struct ext4_super_block *)sb_buf;
   return (sb->s_magic == EXT4_SUPER_MAGIC);
}// ext4_check_magic

int ext4_read_extent_file(const struct ext4_inode *inode, uint32_t block_offset, uint32_t *phys_block) {
   const struct ext4_extent_header *eh = (const struct ext4_extent_header *)inode->i_block;
   if (eh->eh_magic != EXT4_EXTENTS_MAGIC) {
      errorn("ext4: invalid extent header magic");
      return -1;
   }// if

   if (eh->eh_depth == 0) {
      const struct ext4_extent *ext = (const struct ext4_extent *)(inode->i_block + 3);
      for (uint16_t i = 0; i < eh->eh_entries; i++) {
         if (block_offset >= ext[i].ee_block && block_offset < (ext[i].ee_block + ext[i].ee_len)) {
            *phys_block = ext[i].ee_start_lo + (block_offset - ext[i].ee_block);
            return 0;
         }// if
      }// for
   }// if depth 0

   return -1;
}// ext4_read_extent_file

void init_ext4(void) {
   static uint8_t buf[1024];
   infon("Probing ext4 filesystem");
   if (virtio_read_sector(2, buf) != 0 || virtio_read_sector(3, buf + 512) != 0) {
      errorn("ext4: failed to read superblock sectors 2/3");
      return;
   }// if

   struct ext4_super_block *sb = (struct ext4_super_block *)buf;
   if (sb->s_magic == EXT4_SUPER_MAGIC) {
      ext4_block_size = 1024 << sb->s_log_block_size;
      ext4_inode_size = sb->s_inode_size;
      ext4_initialized = true;
      successn("ext4: valid superblock mounted");
   } else {
      errorn("ext4: magic mismatch");
   }// if
}// init_ext4

bool ext4_is_mounted(void) {
   return ext4_initialized;
}// ext4_is_mounted
