/*
 * Ported from the Linux kernel (fs/exfat/exfat_fs.h, super.c)
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

#define EXFAT_OEM_NAME "EXFAT   "

struct exfat_boot_sector {
   uint8_t  jmp_boot[3];
   char     oem_name[8];
   uint8_t  reserved1[53];
   uint64_t partition_offset;
   uint64_t volume_length;
   uint32_t fat_offset;
   uint32_t fat_length;
   uint32_t cluster_heap_offset;
   uint32_t cluster_count;
   uint32_t root_dir_cluster;
   uint32_t volume_serial_number;
   uint16_t fs_revision;
   uint16_t volume_flags;
   uint8_t  bytes_per_sector_shift;
   uint8_t  sectors_per_cluster_shift;
   uint8_t  number_of_fats;
   uint8_t  drive_select;
   uint8_t  percent_in_use;
   uint8_t  reserved2[7];
   uint8_t  boot_code[390];
   uint16_t signature;
} __attribute__((packed));

static bool exfat_mounted = false;
static uint32_t exfat_bytes_per_sector = 512;
static uint32_t exfat_sectors_per_cluster = 8;

extern int virtio_read_sector(uint64_t sector, void *buffer);

void init_exfat(void) {
   static uint8_t boot_buf[512];
   infon("Probing exFAT (Extended FAT) filesystem");
   if (virtio_read_sector(0, boot_buf) != 0) {
      errorn("exfat: failed reading boot sector 0");
      return;
   }// if

   struct exfat_boot_sector *bs = (struct exfat_boot_sector *)boot_buf;
   bool match = true;
   for (int i = 0; i < 8; i++) {
      if (bs->oem_name[i] != EXFAT_OEM_NAME[i]) {
         match = false;
         break;
      }// if
   }// for

   if (match && bs->signature == 0xAA55) {
      exfat_bytes_per_sector = 1 << bs->bytes_per_sector_shift;
      exfat_sectors_per_cluster = 1 << bs->sectors_per_cluster_shift;
      exfat_mounted = true;
      successn("exfat: valid exFAT VBR detected, filesystem mounted");
   } else {
      errorn("exfat: signature mismatch");
   }// if
}// init_exfat

bool exfat_is_mounted(void) {
   return exfat_mounted;
}// exfat_is_mounted
