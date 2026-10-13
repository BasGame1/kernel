#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../include/kernel.h"
#include "../include/gid.h"
#include "../include/drivers.h"

#define EXFAT_OEM_NAME "EXFAT   "
#define EXFAT_ENTRY_EOD 0x00
#define EXFAT_ENTRY_FILE 0x85
#define EXFAT_ENTRY_STREAM 0xC0
#define EXFAT_ENTRY_NAME 0xC1
#define EXFAT_FLAG_ALLOC_POSSIBLE 0x01
#define EXFAT_FLAG_NO_FAT_CHAIN 0x02
#define EXFAT_ATTR_READONLY 0x01
#define EXFAT_ATTR_HIDDEN 0x02
#define EXFAT_ATTR_SYSTEM 0x04
#define EXFAT_ATTR_DIRECTORY 0x10
#define EXFAT_ATTR_ARCHIVE 0x20
#define EXFAT_MAX_PATH_LEN 256
#define EXFAT_MAX_PATH_DEPTH 32
#define EXFAT_MAX_CLUSTER_SIZE 65536
#define EXFAT_MAX_SECONDARY_COUNT 18

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

struct exfat_file_entry {
   uint8_t  entry_type;
   uint8_t  secondary_count;
   uint16_t set_checksum;
   uint16_t file_attributes;
   uint16_t reserved1;
   uint32_t create_timestamp;
   uint32_t modify_timestamp;
   uint32_t access_timestamp;
   uint8_t  create_10ms;
   uint8_t  modify_10ms;
   uint8_t  create_tz;
   uint8_t  modify_tz;
   uint8_t  access_tz;
   uint8_t  reserved2[7];
} __attribute__((packed));

struct exfat_stream_entry {
   uint8_t  entry_type;
   uint8_t  flags;
   uint8_t  reserved1;
   uint8_t  name_length;
   uint16_t name_hash;
   uint16_t reserved2;
   uint64_t valid_data_length;
   uint32_t reserved3;
   uint32_t first_cluster;
   uint64_t data_length;
} __attribute__((packed));

struct exfat_name_entry {
   uint8_t  entry_type;
   uint8_t  flags;
   uint16_t file_name[15];
} __attribute__((packed));

struct exfat_file_info {
   uint32_t first_cluster;
   uint64_t data_length;
   uint16_t attributes;
   bool     no_fat_chain;
};

static bool exfat_mounted = false;
static uint32_t exfat_bytes_per_sector = 512;
static uint32_t exfat_sectors_per_cluster = 8;
static uint32_t exfat_fat_offset = 0;
static uint32_t exfat_fat_length = 0;
static uint32_t exfat_cluster_heap_offset = 0;
static uint32_t exfat_cluster_count = 0;
static uint32_t exfat_root_dir_cluster = 2;
static uint8_t exfat_bytes_per_sector_shift = 9;
static uint8_t exfat_sectors_per_cluster_shift = 3;

static uint8_t exfat_dir_cluster_buf[EXFAT_MAX_CLUSTER_SIZE] __attribute__((aligned(8)));
static uint8_t exfat_file_cluster_buf[EXFAT_MAX_CLUSTER_SIZE] __attribute__((aligned(8)));

extern int virtio_read_sector(uint64_t sector, void *buffer);

uint64_t exfat_cluster_to_sector(uint32_t cluster) {
   if (cluster < 2 || cluster >= 0xFFFFFFF7) {
      return 0;
   }// if
   if (exfat_cluster_count == 0 || (cluster - 2) >= exfat_cluster_count) {
      return 0;
   }// if
   if (exfat_sectors_per_cluster == 0) {
      return 0;
   }// if
   uint64_t rel_sec = (uint64_t)(cluster - 2) * (uint64_t)exfat_sectors_per_cluster;
   return (uint64_t)exfat_cluster_heap_offset + rel_sec;
}// exfat_cluster_to_sector

int exfat_read_cluster(uint32_t cluster, void *buffer) {
   if (!buffer || !exfat_mounted) {
      return -1;
   }// if
   uint64_t start_sec = exfat_cluster_to_sector(cluster);
   if (start_sec == 0) {
      return -1;
   }// if

   for (uint32_t i = 0; i < exfat_sectors_per_cluster; i++) {
      if (virtio_read_sector(start_sec + (uint64_t)i, (uint8_t *)buffer + (i * 512)) != 0) {
         errorn("exfat: cluster read failure");
         return -1;
      }// if
   }// for
   return 0;
}// exfat_read_cluster

uint32_t exfat_get_next_cluster(uint32_t cluster) {
   if (!exfat_mounted || cluster < 2 || cluster >= 0xFFFFFFF7) {
      return 0xFFFFFFFF;
   }// if
   if (exfat_cluster_count == 0 || (cluster - 2) >= exfat_cluster_count) {
      return 0xFFFFFFFF;
   }// if
   if (cluster >= 0x40000000) {
      return 0xFFFFFFFF;
   }// if

   uint64_t fat_offset_bytes = (uint64_t)cluster * 4ULL;
   uint32_t fat_sector_rel = (uint32_t)(fat_offset_bytes / 512ULL);
   uint32_t entry_offset = (uint32_t)(fat_offset_bytes % 512ULL);

   if (exfat_fat_length == 0 || fat_sector_rel >= exfat_fat_length) {
      return 0xFFFFFFFF;
   }// if

   uint64_t fat_sector = (uint64_t)exfat_fat_offset + (uint64_t)fat_sector_rel;

   static uint8_t sector_buf[512] __attribute__((aligned(8)));
   if (virtio_read_sector(fat_sector, sector_buf) != 0) {
      errorn("exfat: FAT sector read failure");
      return 0xFFFFFFFF;
   }// if

   uint32_t next = 0xFFFFFFFF;
   memcpy(&next, sector_buf + entry_offset, sizeof(uint32_t));
   return next;
}// exfat_get_next_cluster

static void exfat_unicode_to_ascii(const uint16_t *src, char *dst, size_t count) {
   if (!src || !dst) {
      return;
   }// if
   for (size_t i = 0; i < count; i++) {
      dst[i] = (src[i] > 0 && src[i] < 0x80) ? (char)src[i] : '?';
   }// for
   dst[count] = '\0';
}// exfat_unicode_to_ascii

int exfat_lookup(uint32_t dir_cluster, const char *name, struct exfat_file_info *out) {
   if (!exfat_mounted || !name || !out) {
      return -1;
   }// if
   if (dir_cluster < 2 || (dir_cluster - 2) >= exfat_cluster_count) {
      return -1;
   }// if

   size_t target_len = strlen(name);
   if (target_len == 0 || target_len > 255) {
      return -1;
   }// if

   uint32_t current_cluster = dir_cluster;
   uint32_t clusters_visited = 0;

   while (current_cluster >= 2 && (current_cluster - 2) < exfat_cluster_count && clusters_visited < exfat_cluster_count) {
      clusters_visited++;
      if (exfat_read_cluster(current_cluster, exfat_dir_cluster_buf) != 0) {
         return -1;
      }// if

      uint32_t entries_per_cluster = (exfat_sectors_per_cluster * 512) / 32;
      for (uint32_t i = 0; i < entries_per_cluster; i++) {
         const uint8_t *entry_raw = exfat_dir_cluster_buf + (i * 32);
         uint8_t type = entry_raw[0];

         if (type == EXFAT_ENTRY_EOD) {
            return -1;
         }// if
         if (type != EXFAT_ENTRY_FILE) {
            continue;
         }// if

         const struct exfat_file_entry *fe = (const struct exfat_file_entry *)entry_raw;
         uint8_t sec_count = fe->secondary_count;
         if (sec_count < 2 || sec_count > EXFAT_MAX_SECONDARY_COUNT || i + (uint32_t)sec_count >= entries_per_cluster) {
            continue;
         }// if

         const struct exfat_stream_entry *se = (const struct exfat_stream_entry *)(exfat_dir_cluster_buf + ((i + 1) * 32));
         if (se->entry_type != EXFAT_ENTRY_STREAM) {
            i += sec_count;
            continue;
         }// if

         if (se->name_length == 0) {
            i += sec_count;
            continue;
         }// if

         uint8_t min_sec = 1 + (uint8_t)((se->name_length + 14) / 15);
         if (sec_count < min_sec) {
            i += sec_count;
            continue;
         }// if

         if (se->data_length > 0 && (se->first_cluster < 2 || (se->first_cluster - 2) >= exfat_cluster_count)) {
            i += sec_count;
            continue;
         }// if

         char name_buf[256];
         size_t name_pos = 0;
         uint8_t name_entries = min_sec - 1;
         bool name_valid = true;

         for (uint8_t n = 0; n < name_entries; n++) {
            const struct exfat_name_entry *ne = (const struct exfat_name_entry *)(exfat_dir_cluster_buf + ((i + 2 + (uint32_t)n) * 32));
            if (ne->entry_type != EXFAT_ENTRY_NAME) {
               name_valid = false;
               break;
            }// if

            uint16_t fname[15];
            memcpy(fname, ne->file_name, sizeof(fname));

            char part[16];
            size_t c = 0;
            while (c < 15 && fname[c] != 0) {
               c++;
            }// while
            exfat_unicode_to_ascii(fname, part, c);
            for (size_t k = 0; k < c && name_pos < sizeof(name_buf) - 1; k++) {
               name_buf[name_pos++] = part[k];
            }// for
         }// for
         name_buf[name_pos] = '\0';

         if (name_valid && name_pos == se->name_length && name_pos == target_len && memcmp(name_buf, name, target_len) == 0) {
            out->first_cluster = se->first_cluster;
            out->data_length = se->data_length;
            out->attributes = fe->file_attributes;
            out->no_fat_chain = (se->flags & EXFAT_FLAG_NO_FAT_CHAIN) != 0;
            return 0;
         }// if

         i += sec_count;
      }// for

      current_cluster = exfat_get_next_cluster(current_cluster);
   }// while

   return -1;
}// exfat_lookup

int exfat_lookup_dir(uint32_t dir_cluster, const char *name, struct exfat_file_info *out) {
   return exfat_lookup(dir_cluster, name, out);
}// exfat_lookup_dir

int exfat_resolve_path(const char *path, struct exfat_file_info *out) {
   if (!exfat_mounted || !path || !out || path[0] != '/') {
      return -1;
   }// if
   if (strlen(path) >= EXFAT_MAX_PATH_LEN) {
      return -1;
   }// if

   out->first_cluster = exfat_root_dir_cluster;
   out->data_length = 0;
   out->attributes = EXFAT_ATTR_DIRECTORY;
   out->no_fat_chain = false;

   const char *p = path + 1;
   uint32_t current_cluster = exfat_root_dir_cluster;
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

      if (++depth > EXFAT_MAX_PATH_DEPTH) {
         return -1;
      }// if

      struct exfat_file_info next_fi;
      if (exfat_lookup(current_cluster, segment, &next_fi) != 0) {
         return -1;
      }// if

      bool had_trailing_slash = false;
      while (*p == '/') {
         had_trailing_slash = true;
         p++;
      }// while

      if (had_trailing_slash && !(next_fi.attributes & EXFAT_ATTR_DIRECTORY)) {
         return -1;
      }// if

      current_cluster = next_fi.first_cluster;
      *out = next_fi;
   }// while

   return 0;
}// exfat_resolve_path

int exfat_read(const char *path, void *buffer, uint32_t count, uint32_t offset) {
   if (!exfat_mounted || !path || !buffer) {
      return -1;
   }// if
   if (count == 0) {
      return 0;
   }// if

   struct exfat_file_info fi;
   if (exfat_resolve_path(path, &fi) != 0) {
      return -1;
   }// if
   if (fi.attributes & EXFAT_ATTR_DIRECTORY) {
      return -1;
   }// if

   if (offset >= fi.data_length) {
      return 0;
   }// if

   uint64_t remaining = fi.data_length - (uint64_t)offset;
   uint32_t available = (remaining > 0x7FFFFFFF) ? 0x7FFFFFFF : (uint32_t)remaining;
   uint32_t to_read = count < available ? count : available;
   if (to_read == 0) {
      return 0;
   }// if

   uint32_t cluster_bytes = exfat_sectors_per_cluster * 512;
   if (cluster_bytes == 0) {
      return -1;
   }// if

   uint32_t skip_clusters = offset / cluster_bytes;
   uint32_t in_cluster_offset = offset % cluster_bytes;

   if (skip_clusters >= exfat_cluster_count) {
      return -1;
   }// if

   uint32_t cur_cluster = fi.first_cluster;
   if (cur_cluster < 2 || (cur_cluster - 2) >= exfat_cluster_count) {
      return -1;
   }// if

   for (uint32_t i = 0; i < skip_clusters; i++) {
      if (fi.no_fat_chain) {
         cur_cluster++;
      }// if
      else {
         cur_cluster = exfat_get_next_cluster(cur_cluster);
      }// else
      if (cur_cluster < 2 || (cur_cluster - 2) >= exfat_cluster_count) {
         return -1;
      }// if
   }// for

   uint32_t bytes_copied = 0;
   uint32_t clusters_read = 0;

   while (bytes_copied < to_read && cur_cluster >= 2 && (cur_cluster - 2) < exfat_cluster_count && clusters_read < exfat_cluster_count) {
      clusters_read++;
      if (exfat_read_cluster(cur_cluster, exfat_file_cluster_buf) != 0) {
         return -1;
      }// if

      uint32_t chunk = cluster_bytes - in_cluster_offset;
      if (to_read - bytes_copied < chunk) {
         chunk = to_read - bytes_copied;
      }// if

      memcpy((uint8_t *)buffer + bytes_copied, exfat_file_cluster_buf + in_cluster_offset, chunk);
      bytes_copied += chunk;
      in_cluster_offset = 0;

      if (bytes_copied >= to_read) {
         break;
      }// if

      if (fi.no_fat_chain) {
         cur_cluster++;
      }// if
      else {
         cur_cluster = exfat_get_next_cluster(cur_cluster);
      }// else
   }// while

   return (int)bytes_copied;
}// exfat_read

int exfat_list_dir(const char *path) {
   if (!exfat_mounted || !path) {
      return -1;
   }// if

   struct exfat_file_info fi;
   if (exfat_resolve_path(path, &fi) != 0) {
      return -1;
   }// if
   if (!(fi.attributes & EXFAT_ATTR_DIRECTORY)) {
      return -1;
   }// if
   if (fi.first_cluster < 2 || (fi.first_cluster - 2) >= exfat_cluster_count) {
      return -1;
   }// if

   infon("exfat: directory contents:");
   uint32_t current_cluster = fi.first_cluster;
   uint32_t clusters_visited = 0;

   while (current_cluster >= 2 && (current_cluster - 2) < exfat_cluster_count && clusters_visited < exfat_cluster_count) {
      clusters_visited++;
      if (exfat_read_cluster(current_cluster, exfat_dir_cluster_buf) != 0) {
         return -1;
      }// if

      uint32_t entries_per_cluster = (exfat_sectors_per_cluster * 512) / 32;
      for (uint32_t i = 0; i < entries_per_cluster; i++) {
         const uint8_t *raw = exfat_dir_cluster_buf + (i * 32);
         if (raw[0] == EXFAT_ENTRY_EOD) {
            return 0;
         }// if
         if (raw[0] != EXFAT_ENTRY_FILE) {
            continue;
         }// if

         const struct exfat_file_entry *fe = (const struct exfat_file_entry *)raw;
         uint8_t sec_count = fe->secondary_count;
         if (sec_count < 2 || sec_count > EXFAT_MAX_SECONDARY_COUNT || i + (uint32_t)sec_count >= entries_per_cluster) {
            continue;
         }// if

         const struct exfat_stream_entry *se = (const struct exfat_stream_entry *)(exfat_dir_cluster_buf + ((i + 1) * 32));
         if (se->entry_type != EXFAT_ENTRY_STREAM) {
            i += sec_count;
            continue;
         }// if

         if (se->name_length == 0) {
            i += sec_count;
            continue;
         }// if

         uint8_t min_sec = 1 + (uint8_t)((se->name_length + 14) / 15);
         if (sec_count < min_sec) {
            i += sec_count;
            continue;
         }// if

         char name_buf[256];
         size_t name_pos = 0;
         uint8_t name_entries = min_sec - 1;
         bool name_valid = true;

         for (uint8_t n = 0; n < name_entries; n++) {
            const struct exfat_name_entry *ne = (const struct exfat_name_entry *)(exfat_dir_cluster_buf + ((i + 2 + (uint32_t)n) * 32));
            if (ne->entry_type != EXFAT_ENTRY_NAME) {
               name_valid = false;
               break;
            }// if

            uint16_t fname[15];
            memcpy(fname, ne->file_name, sizeof(fname));

            char part[16];
            size_t c = 0;
            while (c < 15 && fname[c] != 0) {
               c++;
            }// while
            exfat_unicode_to_ascii(fname, part, c);
            for (size_t k = 0; k < c && name_pos < sizeof(name_buf) - 1; k++) {
               name_buf[name_pos++] = part[k];
            }// for
         }// for
         name_buf[name_pos] = '\0';

         if (name_valid && name_pos == se->name_length) {
            print_character("  ");
            print_character(name_buf);
            print_character("\n");
         }// if

         i += sec_count;
      }// for

      current_cluster = exfat_get_next_cluster(current_cluster);
   }// while

   return 0;
}// exfat_list_dir

void init_exfat(void) {
   exfat_mounted = false;
   static uint8_t boot_buf[512] __attribute__((aligned(8)));
   infon("exfat: probing boot sector 0");

   if (virtio_read_sector(0, boot_buf) != 0) {
      errorn("exfat: failed reading boot sector");
      return;
   }// if

   struct exfat_boot_sector *bs = (struct exfat_boot_sector *)boot_buf;
   if (memcmp(bs->oem_name, EXFAT_OEM_NAME, 8) != 0 || bs->signature != 0xAA55) {
      errorn("exfat: signature mismatch");
      return;
   }// if

   if (bs->bytes_per_sector_shift != 9) {
      errorn("exfat: invalid sector shift");
      return;
   }// if

   if (bs->sectors_per_cluster_shift > 7 || (bs->bytes_per_sector_shift + bs->sectors_per_cluster_shift) > 25) {
      errorn("exfat: invalid cluster shift");
      return;
   }// if

   uint32_t cluster_size = (1U << bs->bytes_per_sector_shift) * (1U << bs->sectors_per_cluster_shift);
   if (cluster_size == 0 || cluster_size > EXFAT_MAX_CLUSTER_SIZE) {
      errorn("exfat: unsupported cluster size");
      return;
   }// if

   if (bs->fat_offset < 24 || bs->fat_length == 0) {
      errorn("exfat: invalid FAT geometry");
      return;
   }// if

   if ((uint64_t)bs->fat_offset + (uint64_t)bs->fat_length > 0xFFFFFFFFULL) {
      errorn("exfat: FAT offset overflow");
      return;
   }// if

   if (bs->cluster_heap_offset < bs->fat_offset + bs->fat_length) {
      errorn("exfat: invalid cluster heap offset");
      return;
   }// if

   if (bs->cluster_count == 0 || bs->cluster_count > 0xFFFFFFF5) {
      errorn("exfat: invalid cluster count");
      return;
   }// if

   if (bs->root_dir_cluster < 2 || (bs->root_dir_cluster - 2) >= bs->cluster_count) {
      errorn("exfat: invalid root directory cluster");
      return;
   }// if

   if (bs->number_of_fats == 0 || bs->number_of_fats > 2) {
      errorn("exfat: invalid number of FATs");
      return;
   }// if

   if (bs->volume_length == 0) {
      errorn("exfat: invalid volume length");
      return;
   }// if

   uint64_t required_heap_sectors = (uint64_t)bs->cluster_count * (uint64_t)(1U << bs->sectors_per_cluster_shift);
   if ((uint64_t)bs->cluster_heap_offset + required_heap_sectors > bs->volume_length) {
      errorn("exfat: volume length smaller than cluster heap");
      return;
   }// if

   exfat_bytes_per_sector_shift = bs->bytes_per_sector_shift;
   exfat_sectors_per_cluster_shift = bs->sectors_per_cluster_shift;
   exfat_bytes_per_sector = 1U << bs->bytes_per_sector_shift;
   exfat_sectors_per_cluster = 1U << bs->sectors_per_cluster_shift;
   exfat_fat_offset = bs->fat_offset;
   exfat_fat_length = bs->fat_length;
   exfat_cluster_heap_offset = bs->cluster_heap_offset;
   exfat_cluster_count = bs->cluster_count;
   exfat_root_dir_cluster = bs->root_dir_cluster;
   exfat_mounted = true;

   successn("exfat: filesystem detected and mounted successfully");
}// init_exfat

bool exfat_is_mounted(void) {
   return exfat_mounted;
}// exfat_is_mounted
