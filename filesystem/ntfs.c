/*
 * Ported from the Linux kernel (fs/ntfs3/super.c, fs/ntfs/layout.h)
 * Copyright (C) Linus Torvalds and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/drivers.h"

#define NTFS_OEM_ID "NTFS    "
#define NTFS_RECORD_MAGIC 0x454C4946

struct ntfs_boot_sector {
   uint8_t  jmp_boot[3];
   char     oem_id[8];
   uint16_t bytes_per_sector;
   uint8_t  sectors_per_cluster;
   uint16_t reserved_sectors;
   uint8_t  fats;
   uint16_t root_entries;
   uint16_t sectors_16;
   uint8_t  media_type;
   uint16_t sectors_per_fat;
   uint16_t sectors_per_track;
   uint16_t heads;
   uint32_t hidden_sectors;
   uint32_t sectors_32;
   uint8_t  bios_drive_num;
   uint8_t  reserved1;
   uint8_t  boot_signature;
   uint8_t  reserved2;
   uint64_t total_sectors;
   uint64_t mft_lcn;
   uint64_t mftmirr_lcn;
   int8_t   clusters_per_mft_record;
   uint8_t  reserved3[3];
   int8_t   clusters_per_index_record;
   uint8_t  reserved4[3];
   uint64_t volume_serial_number;
   uint32_t checksum;
} __attribute__((packed));

struct ntfs_record_header {
   uint32_t magic;
   uint16_t usa_ofs;
   uint16_t usa_count;
   uint64_t lsn;
   uint16_t seq_num;
   uint16_t link_count;
   uint16_t attrs_offset;
   uint16_t flags;
   uint32_t bytes_in_use;
   uint32_t bytes_allocated;
   uint64_t base_mft_record;
   uint16_t next_attr_instance;
   uint16_t reserved;
   uint32_t mft_record_number;
} __attribute__((packed));

struct ntfs_attr_record {
   uint32_t type;
   uint32_t length;
   uint8_t  non_resident;
   uint8_t  name_length;
   uint16_t name_offset;
   uint16_t flags;
   uint16_t instance;
} __attribute__((packed));

static bool ntfs_mounted = false;
static uint32_t ntfs_bytes_per_cluster = 4096;
static uint64_t ntfs_mft_start_lcn = 0;

extern int virtio_read_sector(uint64_t sector, void *buffer);

bool ntfs_is_valid_boot(const void *boot_buf) {
   const struct ntfs_boot_sector *bs = (const struct ntfs_boot_sector *)boot_buf;
   for (int i = 0; i < 8; i++) {
      if (bs->oem_id[i] != NTFS_OEM_ID[i]) {
         return false;
      }// if
   }// for
   return true;
}// ntfs_is_valid_boot

int ntfs_read_mft_record(uint32_t record_num, void *buf) {
   uint32_t mft_sector = (uint32_t)(ntfs_mft_start_lcn * (ntfs_bytes_per_cluster / 512));
   uint32_t target_sector = mft_sector + (record_num * 2);
   if (virtio_read_sector(target_sector, buf) != 0 || virtio_read_sector(target_sector + 1, (uint8_t *)buf + 512) != 0) {
      errorn("ntfs: failed reading MFT record sector");
      return -1;
   }// if
   const struct ntfs_record_header *rh = (const struct ntfs_record_header *)buf;
   if (rh->magic != NTFS_RECORD_MAGIC) {
      errorn("ntfs: invalid MFT record magic");
      return -1;
   }// if
   return 0;
}// ntfs_read_mft_record

void init_ntfs(void) {
   static uint8_t sector_buf[512];
   infon("Probing NTFS filesystem");
   if (virtio_read_sector(0, sector_buf) != 0) {
      errorn("ntfs: failed to read boot sector 0");
      return;
   }// if
   struct ntfs_boot_sector *bs = (struct ntfs_boot_sector *)sector_buf;
   if (ntfs_is_valid_boot(bs)) {
      ntfs_bytes_per_cluster = bs->bytes_per_sector * bs->sectors_per_cluster;
      ntfs_mft_start_lcn = bs->mft_lcn;
      ntfs_mounted = true;
      successn("ntfs: valid boot sector detected, filesystem mounted");
   } else {
      errorn("ntfs: signature mismatch");
   }// if
}// init_ntfs

bool ntfs_is_mounted(void) {
   return ntfs_mounted;
}// ntfs_is_mounted
