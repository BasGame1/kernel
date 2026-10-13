#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../include/kernel.h"
#include "../include/gid.h"
#include "../include/drivers.h"

#define VFAT_ATTR_READ_ONLY 0x01
#define VFAT_ATTR_HIDDEN 0x02
#define VFAT_ATTR_SYSTEM 0x04
#define VFAT_ATTR_VOLUME_ID 0x08
#define VFAT_ATTR_DIRECTORY 0x10
#define VFAT_ATTR_ARCHIVE 0x20
#define VFAT_ATTR_LFN 0x0F
#define VFAT_ENTRY_FREE 0xE5
#define VFAT_ENTRY_END 0x00
#define VFAT_FAT32_EOC 0x0FFFFFF8
#define VFAT_FAT32_BAD 0x0FFFFFF7
#define VFAT_FAT16_EOC 0xFFF8
#define VFAT_FAT16_BAD 0xFFF7
#define VFAT_MAX_PATH_LEN 256
#define VFAT_MAX_PATH_DEPTH 32
#define VFAT_MAX_CLUSTER_SIZE 65536

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
static uint32_t vfat_fat_length = 0;
static uint32_t vfat_total_sectors = 0;
static uint32_t vfat_total_clusters = 0;
static uint32_t vfat_root_cluster = 2;
static bool vfat_is_fat32 = true;

static uint8_t vfat_dir_cluster_buf[VFAT_MAX_CLUSTER_SIZE] __attribute__((aligned(8)));
static uint8_t vfat_file_cluster_buf[VFAT_MAX_CLUSTER_SIZE] __attribute__((aligned(8)));

extern int virtio_read_sector(uint64_t sector, void *buffer);

uint32_t vfat_cluster_to_sector(uint32_t cluster) {
   if (!vfat_mounted || cluster < 2) {
      return 0;
   }// if
   if (vfat_total_clusters == 0 || (cluster - 2) >= vfat_total_clusters) {
      return 0;
   }// if
   if (vfat_sec_per_clus == 0) {
      return 0;
   }// if
   uint64_t rel_sec = (uint64_t)(cluster - 2) * (uint64_t)vfat_sec_per_clus;
   uint64_t sec = (uint64_t)vfat_first_data_sector + rel_sec;
   if (sec >= (uint64_t)vfat_total_sectors || sec > 0xFFFFFFFFULL) {
      return 0;
   }// if
   return (uint32_t)sec;
}// vfat_cluster_to_sector

int vfat_read_cluster(uint32_t cluster, void *buffer) {
   if (!vfat_mounted || !buffer) {
      return -1;
   }// if
   if (vfat_sec_per_clus == 0 || vfat_sec_per_clus > 128) {
      return -1;
   }// if
   if ((uint32_t)vfat_sec_per_clus * 512U > VFAT_MAX_CLUSTER_SIZE) {
      return -1;
   }// if
   uint32_t start_sec = vfat_cluster_to_sector(cluster);
   if (start_sec == 0) {
      return -1;
   }// if

   for (uint32_t i = 0; i < vfat_sec_per_clus; i++) {
      uint64_t target_sec = (uint64_t)start_sec + (uint64_t)i;
      if (target_sec >= (uint64_t)vfat_total_sectors || target_sec > 0xFFFFFFFFULL) {
         errorn("vfat: cluster sector exceeds volume bounds");
         return -1;
      }// if
      if (virtio_read_sector(target_sec, (uint8_t *)buffer + (i * 512)) != 0) {
         errorn("vfat: failed reading cluster sector");
         return -1;
      }// if
   }// for
   return 0;
}// vfat_read_cluster

uint32_t vfat_get_next_cluster(uint32_t cluster) {
   static uint8_t fat_sector_buf[512] __attribute__((aligned(8)));

   if (!vfat_mounted || cluster < 2) {
      return 0x0FFFFFFF;
   }// if
   if (vfat_total_clusters == 0 || (cluster - 2) >= vfat_total_clusters) {
      return 0x0FFFFFFF;
   }// if
   if (cluster >= 0x10000000) {
      return 0x0FFFFFFF;
   }// if

   if (vfat_is_fat32) {
      uint64_t fat_offset = (uint64_t)cluster * 4ULL;
      uint32_t fat_sec_rel = (uint32_t)(fat_offset / 512ULL);
      uint32_t in_sector_offset = (uint32_t)(fat_offset % 512ULL);

      if (vfat_fat_length == 0 || fat_sec_rel >= vfat_fat_length) {
         return 0x0FFFFFFF;
      }// if

      uint64_t fat_sector = (uint64_t)vfat_fat_start_sector + (uint64_t)fat_sec_rel;
      if (fat_sector >= (uint64_t)vfat_total_sectors || fat_sector > 0xFFFFFFFFULL) {
         return 0x0FFFFFFF;
      }// if

      if (virtio_read_sector(fat_sector, fat_sector_buf) != 0) {
         errorn("vfat: failed to read FAT sector");
         return 0x0FFFFFFF;
      }// if

      if (in_sector_offset > 512 - sizeof(uint32_t)) {
         return 0x0FFFFFFF;
      }// if

      uint32_t next = 0x0FFFFFFF;
      memcpy(&next, fat_sector_buf + in_sector_offset, sizeof(uint32_t));
      next &= 0x0FFFFFFF;

      if (next < 2 || next >= VFAT_FAT32_BAD) {
         return 0x0FFFFFFF;
      }// if
      if ((next - 2) >= vfat_total_clusters) {
         return 0x0FFFFFFF;
      }// if
      return next;
   }// if
   else {
      uint64_t fat_offset = (uint64_t)cluster * 2ULL;
      uint32_t fat_sec_rel = (uint32_t)(fat_offset / 512ULL);
      uint32_t in_sector_offset = (uint32_t)(fat_offset % 512ULL);

      if (vfat_fat_length == 0 || fat_sec_rel >= vfat_fat_length) {
         return 0x0FFFFFFF;
      }// if

      uint64_t fat_sector = (uint64_t)vfat_fat_start_sector + (uint64_t)fat_sec_rel;
      if (fat_sector >= (uint64_t)vfat_total_sectors || fat_sector > 0xFFFFFFFFULL) {
         return 0x0FFFFFFF;
      }// if

      if (virtio_read_sector(fat_sector, fat_sector_buf) != 0) {
         return 0x0FFFFFFF;
      }// if

      if (in_sector_offset > 512 - sizeof(uint16_t)) {
         return 0x0FFFFFFF;
      }// if

      uint16_t next = 0xFFFF;
      memcpy(&next, fat_sector_buf + in_sector_offset, sizeof(uint16_t));

      if (next < 2 || next >= VFAT_FAT16_BAD) {
         return 0x0FFFFFFF;
      }// if
      if ((uint32_t)(next - 2) >= vfat_total_clusters) {
         return 0x0FFFFFFF;
      }// if
      return (uint32_t)next;
   }// else
}// vfat_get_next_cluster

static void vfat_format_83_name(const uint8_t *raw, char *out) {
   if (!raw || !out) {
      if (out) {
         out[0] = '\0';
      }// if
      return;
   }// if

   if (raw[0] == ' ') {
      out[0] = '\0';
      return;
   }// if

   int name_len = 8;
   while (name_len > 0 && raw[name_len - 1] == ' ') {
      name_len--;
   }// while

   int ext_len = 3;
   while (ext_len > 0 && raw[8 + ext_len - 1] == ' ') {
      ext_len--;
   }// while

   if (name_len == 0 && ext_len == 0) {
      out[0] = '\0';
      return;
   }// if

   size_t out_idx = 0;
   for (int i = 0; i < name_len; i++) {
      char c = (char)raw[i];
      if (i == 0 && (uint8_t)c == 0x05) {
         c = (char)0xE5;
      }// if
      if (c >= 'A' && c <= 'Z') {
         c += ('a' - 'A');
      }// if
      if (out_idx < 15) {
         out[out_idx++] = c;
      }// if
   }// for

   if (ext_len > 0) {
      if (out_idx < 15) {
         out[out_idx++] = '.';
      }// if
      for (int i = 0; i < ext_len; i++) {
         char c = (char)raw[8 + i];
         if (c >= 'A' && c <= 'Z') {
            c += ('a' - 'A');
         }// if
         if (out_idx < 15) {
            out[out_idx++] = c;
         }// if
      }// for
   }// if

   out[out_idx] = '\0';
}// vfat_format_83_name

static bool vfat_name_match(const char *name1, const char *name2) {
   if (!name1 || !name2) {
      return false;
   }// if
   if (*name1 == '\0' || *name2 == '\0') {
      return false;
   }// if
   while (*name1 && *name2) {
      char c1 = *name1;
      char c2 = *name2;
      if (c1 >= 'A' && c1 <= 'Z') {
         c1 += ('a' - 'A');
      }// if
      if (c2 >= 'A' && c2 <= 'Z') {
         c2 += ('a' - 'A');
      }// if
      if (c1 != c2) {
         return false;
      }// if
      name1++;
      name2++;
   }// while
   return (*name1 == '\0' && *name2 == '\0');
}// vfat_name_match

int vfat_lookup(uint32_t dir_cluster, const char *name, struct vfat_dir_entry *out) {
   if (!vfat_mounted || !name || !out || *name == '\0') {
      return -1;
   }// if

   uint32_t current_cluster = dir_cluster;
   if (current_cluster == 0) {
      current_cluster = vfat_root_cluster;
   }// if

   if (current_cluster < 2 || (current_cluster - 2) >= vfat_total_clusters) {
      return -1;
   }// if

   uint32_t max_clusters = (vfat_total_clusters > 0 && vfat_total_clusters < 65536) ? vfat_total_clusters : 65536;
   uint32_t clusters_visited = 0;

   while (current_cluster >= 2 && (current_cluster - 2) < vfat_total_clusters && clusters_visited < max_clusters) {
      clusters_visited++;
      if (vfat_read_cluster(current_cluster, vfat_dir_cluster_buf) != 0) {
         return -1;
      }// if

      uint32_t entries_per_cluster = ((uint32_t)vfat_sec_per_clus * 512U) / sizeof(struct vfat_dir_entry);
      if (entries_per_cluster > VFAT_MAX_CLUSTER_SIZE / sizeof(struct vfat_dir_entry)) {
         entries_per_cluster = VFAT_MAX_CLUSTER_SIZE / sizeof(struct vfat_dir_entry);
      }// if

      const struct vfat_dir_entry *entries = (const struct vfat_dir_entry *)vfat_dir_cluster_buf;

      for (uint32_t i = 0; i < entries_per_cluster; i++) {
         if (entries[i].name[0] == VFAT_ENTRY_END) {
            return -1;
         }// if
         if (entries[i].name[0] == VFAT_ENTRY_FREE) {
            continue;
         }// if
         if (entries[i].attr == VFAT_ATTR_LFN) {
            continue;
         }// if
         if (entries[i].attr & VFAT_ATTR_VOLUME_ID) {
            continue;
         }// if

         char formatted[16];
         vfat_format_83_name(entries[i].name, formatted);

         if (vfat_name_match(formatted, name)) {
            memcpy(out, &entries[i], sizeof(struct vfat_dir_entry));
            return 0;
         }// if
      }// for

      current_cluster = vfat_get_next_cluster(current_cluster);
   }// while

   return -1;
}// vfat_lookup

int vfat_lookup_dir(uint32_t dir_cluster, const char *name, struct vfat_dir_entry *out) {
   return vfat_lookup(dir_cluster, name, out);
}// vfat_lookup_dir

int vfat_resolve_path(const char *path, struct vfat_dir_entry *out) {
   if (!vfat_mounted || !path || !out) {
      return -1;
   }// if
   if (path[0] != '/') {
      return -1;
   }// if

   size_t path_len = strlen(path);
   if (path_len == 0 || path_len >= VFAT_MAX_PATH_LEN) {
      return -1;
   }// if

   memset(out, 0, sizeof(struct vfat_dir_entry));
   out->starthi = (uint16_t)(vfat_root_cluster >> 16);
   out->startlo = (uint16_t)(vfat_root_cluster & 0xFFFF);
   out->size = 0;
   out->attr = VFAT_ATTR_DIRECTORY;

   const char *p = path + 1;
   uint32_t current_cluster = vfat_root_cluster;
   uint32_t depth = 0;

   while (*p != '\0') {
      while (*p == '/') {
         p++;
      }// while
      if (*p == '\0') {
         break;
      }// if

      char segment[256];
      size_t len = 0;
      while (*p != '/' && *p != '\0') {
         if (len >= sizeof(segment) - 1) {
            return -1;
         }// if
         segment[len++] = *p++;
      }// while
      segment[len] = '\0';

      if (len == 1 && segment[0] == '.') {
         continue;
      }// if
      if (len == 2 && segment[0] == '.' && segment[1] == '.') {
         return -1;
      }// if

      if (++depth > VFAT_MAX_PATH_DEPTH) {
         return -1;
      }// if

      struct vfat_dir_entry entry;
      if (vfat_lookup(current_cluster, segment, &entry) != 0) {
         return -1;
      }// if

      bool had_trailing_slash = false;
      while (*p == '/') {
         had_trailing_slash = true;
         p++;
      }// while

      if (had_trailing_slash && !(entry.attr & VFAT_ATTR_DIRECTORY)) {
         return -1;
      }// if

      if (*p != '\0' && !(entry.attr & VFAT_ATTR_DIRECTORY)) {
         return -1;
      }// if

      current_cluster = ((uint32_t)entry.starthi << 16) | (uint32_t)entry.startlo;
      if (current_cluster == 0) {
         current_cluster = vfat_root_cluster;
      }// if
      *out = entry;
   }// while

   return 0;
}// vfat_resolve_path

int vfat_read(const char *path, void *buffer, uint32_t count, uint32_t offset) {
   if (!vfat_mounted || !path || !buffer) {
      return -1;
   }// if
   if (count == 0) {
      return 0;
   }// if

   struct vfat_dir_entry entry;
   if (vfat_resolve_path(path, &entry) != 0) {
      return -1;
   }// if

   if (entry.attr & VFAT_ATTR_DIRECTORY) {
      return -1;
   }// if

   if (offset >= entry.size) {
      return 0;
   }// if

   uint64_t remaining = (uint64_t)entry.size - (uint64_t)offset;
   uint32_t available = (remaining > 0x7FFFFFFF) ? 0x7FFFFFFF : (uint32_t)remaining;
   uint32_t to_read = count < available ? count : available;
   if (to_read == 0) {
      return 0;
   }// if

   uint32_t cluster_bytes = (uint32_t)vfat_sec_per_clus * 512U;
   if (cluster_bytes == 0 || cluster_bytes > VFAT_MAX_CLUSTER_SIZE) {
      return -1;
   }// if

   uint32_t skip_clusters = offset / cluster_bytes;
   uint32_t in_cluster_offset = offset % cluster_bytes;

   if (vfat_total_clusters == 0 || skip_clusters >= vfat_total_clusters) {
      return -1;
   }// if

   uint32_t cur_cluster = ((uint32_t)entry.starthi << 16) | (uint32_t)entry.startlo;
   if (cur_cluster < 2 || (cur_cluster - 2) >= vfat_total_clusters) {
      return -1;
   }// if

   uint32_t skip_visited = 0;
   for (uint32_t i = 0; i < skip_clusters; i++) {
      skip_visited++;
      if (skip_visited >= vfat_total_clusters) {
         return -1;
      }// if
      cur_cluster = vfat_get_next_cluster(cur_cluster);
      if (cur_cluster < 2 || (cur_cluster - 2) >= vfat_total_clusters) {
         return -1;
      }// if
   }// for

   uint32_t bytes_copied = 0;
   uint32_t clusters_read = 0;

   while (bytes_copied < to_read && cur_cluster >= 2 && (cur_cluster - 2) < vfat_total_clusters && clusters_read < vfat_total_clusters) {
      clusters_read++;
      if (vfat_read_cluster(cur_cluster, vfat_file_cluster_buf) != 0) {
         return -1;
      }// if

      if (in_cluster_offset >= cluster_bytes) {
         return -1;
      }// if

      uint32_t chunk = cluster_bytes - in_cluster_offset;
      if (to_read - bytes_copied < chunk) {
         chunk = to_read - bytes_copied;
      }// if

      memcpy((uint8_t *)buffer + bytes_copied, vfat_file_cluster_buf + in_cluster_offset, chunk);
      bytes_copied += chunk;
      in_cluster_offset = 0;

      if (bytes_copied >= to_read) {
         break;
      }// if

      cur_cluster = vfat_get_next_cluster(cur_cluster);
   }// while

   return (int)bytes_copied;
}// vfat_read

int vfat_list_dir(const char *path) {
   if (!vfat_mounted || !path) {
      return -1;
   }// if

   struct vfat_dir_entry entry;
   if (vfat_resolve_path(path, &entry) != 0) {
      return -1;
   }// if

   if (!(entry.attr & VFAT_ATTR_DIRECTORY)) {
      return -1;
   }// if

   uint32_t current_cluster = ((uint32_t)entry.starthi << 16) | (uint32_t)entry.startlo;
   if (current_cluster == 0) {
      current_cluster = vfat_root_cluster;
   }// if

   if (current_cluster < 2 || (current_cluster - 2) >= vfat_total_clusters) {
      return -1;
   }// if

   infon("vfat: directory contents:");

   uint32_t max_clusters = (vfat_total_clusters > 0 && vfat_total_clusters < 65536) ? vfat_total_clusters : 65536;
   uint32_t clusters_visited = 0;

   while (current_cluster >= 2 && (current_cluster - 2) < vfat_total_clusters && clusters_visited < max_clusters) {
      clusters_visited++;
      if (vfat_read_cluster(current_cluster, vfat_dir_cluster_buf) != 0) {
         return -1;
      }// if

      uint32_t entries_per_cluster = ((uint32_t)vfat_sec_per_clus * 512U) / sizeof(struct vfat_dir_entry);
      if (entries_per_cluster > VFAT_MAX_CLUSTER_SIZE / sizeof(struct vfat_dir_entry)) {
         entries_per_cluster = VFAT_MAX_CLUSTER_SIZE / sizeof(struct vfat_dir_entry);
      }// if

      const struct vfat_dir_entry *entries = (const struct vfat_dir_entry *)vfat_dir_cluster_buf;

      for (uint32_t i = 0; i < entries_per_cluster; i++) {
         if (entries[i].name[0] == VFAT_ENTRY_END) {
            return 0;
         }// if
         if (entries[i].name[0] == VFAT_ENTRY_FREE) {
            continue;
         }// if
         if (entries[i].attr == VFAT_ATTR_LFN) {
            continue;
         }// if
         if (entries[i].attr & VFAT_ATTR_VOLUME_ID) {
            continue;
         }// if

         char formatted[16];
         vfat_format_83_name(entries[i].name, formatted);
         if (formatted[0] == '\0') {
            continue;
         }// if
         print_character("  ");
         print_character(formatted);
         print_character("\n");
      }// for

      current_cluster = vfat_get_next_cluster(current_cluster);
   }// while

   return 0;
}// vfat_list_dir

void init_vfat(void) {
   vfat_mounted = false;
   vfat_total_clusters = 0;
   vfat_total_sectors = 0;
   vfat_fat_length = 0;
   static uint8_t sector_buf[512] __attribute__((aligned(8)));
   infon("vfat: probing boot sector 0");

   if (virtio_read_sector(0, sector_buf) != 0) {
      errorn("vfat: failed reading boot sector");
      return;
   }// if

   if (sector_buf[510] != 0x55 || sector_buf[511] != 0xAA) {
      errorn("vfat: boot sector signature mismatch");
      return;
   }// if

   struct fat_boot_sector *bs = (struct fat_boot_sector *)sector_buf;
   uint16_t bytes_sec = (uint16_t)bs->sector_size[0] | ((uint16_t)bs->sector_size[1] << 8);

   if (bytes_sec != 512) {
      errorn("vfat: unsupported sector size");
      return;
   }// if

   vfat_bytes_per_sec = 512;
   vfat_sec_per_clus = bs->sec_per_clus;
   if (vfat_sec_per_clus == 0 || vfat_sec_per_clus > 128 || (vfat_sec_per_clus & (vfat_sec_per_clus - 1)) != 0) {
      errorn("vfat: invalid sectors per cluster in BPB");
      return;
   }// if

   if ((uint32_t)vfat_sec_per_clus * 512U > VFAT_MAX_CLUSTER_SIZE) {
      errorn("vfat: cluster size exceeds buffer capacity");
      return;
   }// if

   if (bs->reserved == 0) {
      errorn("vfat: invalid reserved sector count in BPB");
      return;
   }// if

   if (bs->fats == 0 || bs->fats > 2) {
      errorn("vfat: invalid number of FATs in BPB");
      return;
   }// if

   vfat_fat_start_sector = bs->reserved;
   uint32_t fat_sz = bs->fat_length ? (uint32_t)bs->fat_length : bs->fat32_length;
   if (fat_sz == 0) {
      errorn("vfat: invalid FAT size in BPB");
      return;
   }// if
   vfat_fat_length = fat_sz;

   vfat_is_fat32 = (bs->fat_length == 0 && bs->fat32_length != 0);

   uint16_t root_entries = (uint16_t)bs->dir_entries[0] | ((uint16_t)bs->dir_entries[1] << 8);
   uint32_t root_dir_sectors = (((uint32_t)root_entries * 32U) + 511U) / 512U;

   uint64_t fat_total_sec = (uint64_t)bs->reserved + ((uint64_t)bs->fats * (uint64_t)fat_sz);
   if (fat_total_sec > 0xFFFFFFFFULL) {
      errorn("vfat: FAT size overflow");
      return;
   }// if

   uint64_t first_data = fat_total_sec + (uint64_t)root_dir_sectors;
   if (first_data > 0xFFFFFFFFULL) {
      errorn("vfat: first data sector overflow");
      return;
   }// if
   vfat_first_data_sector = (uint32_t)first_data;

   uint32_t total_sec = bs->total_sect ? bs->total_sect : (uint32_t)((uint16_t)bs->sectors[0] | ((uint16_t)bs->sectors[1] << 8));
   if (total_sec == 0 || total_sec <= vfat_first_data_sector) {
      errorn("vfat: invalid total sector count in BPB");
      return;
   }// if
   vfat_total_sectors = total_sec;

   uint32_t data_sectors = total_sec - vfat_first_data_sector;
   vfat_total_clusters = data_sectors / vfat_sec_per_clus;
   if (vfat_total_clusters == 0) {
      errorn("vfat: volume has 0 data clusters");
      return;
   }// if

   if (vfat_total_clusters < 4085) {
      errorn("vfat: unsupported FAT12 filesystem");
      return;
   }// if

   if (vfat_is_fat32) {
      vfat_root_cluster = bs->root_cluster ? bs->root_cluster : 2;
      if (vfat_root_cluster < 2 || (vfat_root_cluster - 2) >= vfat_total_clusters) {
         errorn("vfat: invalid root cluster in BPB");
         return;
      }// if
   }// if
   else {
      vfat_root_cluster = 2;
   }// else

   vfat_mounted = true;
   successn("vfat: valid BPB detected, filesystem mounted");
}// init_vfat

bool vfat_is_mounted(void) {
   return vfat_mounted;
}// vfat_is_mounted
