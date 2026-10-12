/*
 * Ported from the Linux kernel (fs/fat/fat.h, fs/fat/dir.c)
 * Copyright (C) Linus Torvalds and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/drivers.h"

#define VFAT_ATTR_READ_ONLY 0x01
#define VFAT_ATTR_HIDDEN 0x02
#define VFAT_ATTR_SYSTEM 0x04
#define VFAT_ATTR_VOLUME_ID 0x08
#define VFAT_ATTR_DIRECTORY 0x10
#define VFAT_ATTR_ARCHIVE 0x20
#define VFAT_ATTR_LFN 0x0F

struct fat_boot_sector {
   uint8_t  ignored[3];
   char     system_id[8];
   uint8_t  sector_size[2];
   uint8_t  sec_per_clus;
   uint16_t reserved;
   uint8_t  fats;
   uint8_t  dir_entries[2];
   uint8_t  sectors[2];
   uint8_t  media;
   uint16_t fat_length;
   uint16_t secs_track;
   uint16_t heads;
   uint32_t hidden;
   uint32_t total_sect;
   uint32_t fat32_length;
   uint16_t flags;
   uint8_t  version[2];
   uint32_t root_cluster;
   uint16_t info_sector;
   uint16_t backup_boot;
   uint16_t reserved2[6];
} __attribute__((packed));

struct vfat_dir_entry {
   uint8_t  name[11];
   uint8_t  attr;
   uint8_t  lcase;
   uint8_t  ctime_cs;
   uint16_t ctime;
   uint16_t cdate;
   uint16_t adate;
   uint16_t starthi;
   uint16_t time;
   uint16_t date;
   uint16_t startlo;
   uint32_t size;
} __attribute__((packed));

static bool vfat_mounted = false;
static uint32_t vfat_bytes_per_sec = 512;
static uint32_t vfat_sec_per_clus = 1;
static uint32_t vfat_first_data_sector = 0;
static uint32_t vfat_fat_start_sector = 0;

extern int virtio_read_sector(uint64_t sector, void *buffer);

uint32_t vfat_cluster_to_sector(uint32_t cluster) {
   if (cluster < 2) {
      return 0;
   }// if
   return vfat_first_data_sector + ((cluster - 2) * vfat_sec_per_clus);
}// vfat_cluster_to_sector

int vfat_read_cluster(uint32_t cluster, void *buffer) {
   uint32_t start_sec = vfat_cluster_to_sector(cluster);
   for (uint32_t i = 0; i < vfat_sec_per_clus; i++) {
      if (virtio_read_sector(start_sec + i, (uint8_t *)buffer + (i * 512)) != 0) {
         errorn("vfat: failed reading cluster sector");
         return -1;
      }// if
   }// for
   return 0;
}// vfat_read_cluster

void init_vfat(void) {
   static uint8_t sector_buf[512];
   infon("Probing VFAT/FAT32 filesystem");
   if (virtio_read_sector(0, sector_buf) != 0) {
      errorn("vfat: failed reading boot sector");
      return;
   }// if
   struct fat_boot_sector *bs = (struct fat_boot_sector *)sector_buf;
   uint16_t bytes_sec = bs->sector_size[0] | (bs->sector_size[1] << 8);
   if (bytes_sec == 512 || bytes_sec == 1024 || bytes_sec == 2048 || bytes_sec == 4096) {
      vfat_bytes_per_sec = bytes_sec;
      vfat_sec_per_clus = bs->sec_per_clus;
      vfat_fat_start_sector = bs->reserved;
      uint32_t fat_sz = bs->fat_length ? bs->fat_length : bs->fat32_length;
      vfat_first_data_sector = bs->reserved + (bs->fats * fat_sz);
      vfat_mounted = true;
      successn("vfat: valid FAT32 BPB detected, filesystem mounted");
   } else {
      errorn("vfat: invalid sector size in BPB");
   }// if
}// init_vfat

bool vfat_is_mounted(void) {
   return vfat_mounted;
}// vfat_is_mounted
