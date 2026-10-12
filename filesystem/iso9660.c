/*
 * Ported from the Linux kernel (fs/isofs/isofs.h, inode.c)
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

#define ISO_STANDARD_ID "CD001"
#define ISO_VD_PRIMARY 1
#define ISO_VD_END 255

struct iso_volume_descriptor {
   uint8_t  type;
   char     id[5];
   uint8_t  version;
   uint8_t  data[2041];
} __attribute__((packed));

struct iso_primary_descriptor {
   uint8_t  type;
   char     id[5];
   uint8_t  version;
   uint8_t  unused1;
   char     system_id[32];
   char     volume_id[32];
   uint8_t  unused2[8];
   uint32_t volume_space_size_le;
   uint32_t volume_space_size_be;
   uint8_t  unused3[32];
   uint16_t volume_set_size_le;
   uint16_t volume_set_size_be;
   uint16_t volume_seq_number_le;
   uint16_t volume_seq_number_be;
   uint16_t logical_block_size_le;
   uint16_t logical_block_size_be;
} __attribute__((packed));

static bool iso9660_mounted = false;
static uint16_t iso_block_size = 2048;

extern int virtio_read_sector(uint64_t sector, void *buffer);

void init_iso9660(void) {
   static uint8_t sec_buf[2048];
   infon("Probing ISO9660 CD-ROM filesystem (sector 16)");
   /* Sector 16 at 2048 bytes/sec = sector 64 (at 512 bytes) */
   for (int i = 0; i < 4; i++) {
      if (virtio_read_sector(64 + i, sec_buf + (i * 512)) != 0) {
         errorn("iso9660: failed reading volume descriptor sector");
         return;
      }// if
   }// for

   struct iso_primary_descriptor *pvd = (struct iso_primary_descriptor *)sec_buf;
   bool match = true;
   for (int i = 0; i < 5; i++) {
      if (pvd->id[i] != ISO_STANDARD_ID[i]) {
         match = false;
         break;
      }// if
   }// for

   if (match && pvd->type == ISO_VD_PRIMARY) {
      iso_block_size = pvd->logical_block_size_le ? pvd->logical_block_size_le : 2048;
      iso9660_mounted = true;
      successn("iso9660: mounted");
   } else {
      errorn("iso9660: not detected");
   }// if
}// init_iso9660

bool iso9660_is_mounted(void) {
   return iso9660_mounted;
}// iso9660_is_mounted
