#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../include/kernel.h"
#include "../include/gid.h"
#include "../include/drivers.h"

#define NTFS_OEM_ID "NTFS    "
#define NTFS_RECORD_MAGIC 0x454C4946
#define NTFS_ROOT_INO 5
#define NTFS_ATTR_STANDARD_INFO 0x10
#define NTFS_ATTR_FILE_NAME 0x30
#define NTFS_ATTR_DATA 0x80
#define NTFS_ATTR_INDEX_ROOT 0x90
#define NTFS_ATTR_INDEX_ALLOC 0xA0
#define NTFS_ATTR_END 0xFFFFFFFF
#define NTFS_MAX_PATH_LEN 256
#define NTFS_MAX_PATH_DEPTH 32
#define NTFS_MAX_CLUSTER_SIZE 65536

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

struct ntfs_attr_header {
   uint32_t type;
   uint32_t length;
   uint8_t  non_resident;
   uint8_t  name_length;
   uint16_t name_offset;
   uint16_t flags;
   uint16_t instance;
   union {
      struct {
         uint32_t value_length;
         uint16_t value_offset;
         uint8_t  resident_flags;
         uint8_t  reserved;
      } __attribute__((packed)) res;
      struct {
         uint64_t lowest_vcn;
         uint64_t highest_vcn;
         uint16_t mapping_pairs_offset;
         uint8_t  compression_unit;
         uint8_t  reserved[5];
         uint64_t allocated_size;
         uint64_t data_size;
         uint64_t initialized_size;
      } __attribute__((packed)) non_res;
   };
} __attribute__((packed));

struct ntfs_file_name_attr {
   uint64_t parent_directory;
   uint64_t ctime;
   uint64_t mtime;
   uint64_t mft_mtime;
   uint64_t atime;
   uint64_t allocated_size;
   uint64_t real_size;
   uint32_t flags;
   uint32_t reparse_tag;
   uint8_t  file_name_length;
   uint8_t  namespace_type;
   uint16_t file_name[1];
} __attribute__((packed));

struct ntfs_index_root_attr {
   uint32_t attr_type;
   uint32_t collation_rule;
   uint32_t index_record_size;
   uint8_t  clusters_per_index_record;
   uint8_t  reserved[3];
   uint32_t entries_offset;
   uint32_t total_size;
   uint32_t allocated_size;
   uint8_t  flags;
   uint8_t  reserved2[3];
} __attribute__((packed));

struct ntfs_index_entry {
   uint64_t mft_reference;
   uint16_t length;
   uint16_t content_length;
   uint32_t flags;
} __attribute__((packed));

static bool ntfs_mounted = false;
static uint32_t ntfs_bytes_per_sector = 512;
static uint32_t ntfs_sectors_per_cluster = 8;
static uint32_t ntfs_bytes_per_cluster = 4096;
static uint64_t ntfs_mft_start_lcn = 0;
static uint64_t ntfs_total_sectors = 0;
static uint32_t ntfs_mft_record_size = 1024;
static uint8_t ntfs_cluster_buf[NTFS_MAX_CLUSTER_SIZE] __attribute__((aligned(8)));

extern int virtio_read_sector(uint64_t sector, void *buffer);

static inline uint16_t ntfs_get_u16(const void *p) {
   const uint8_t *b = (const uint8_t *)p;
   return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}// ntfs_get_u16

static inline void ntfs_put_u16(void *p, uint16_t v) {
   uint8_t *b = (uint8_t *)p;
   b[0] = (uint8_t)(v & 0xFF);
   b[1] = (uint8_t)((v >> 8) & 0xFF);
}// ntfs_put_u16

static int ntfs_apply_fixup(void *buf, uint32_t size) {
   if (!buf || size < 512 || (size % 512) != 0) {
      return -1;
   }// if
   const struct ntfs_record_header *rh = (const struct ntfs_record_header *)buf;
   if (rh->usa_ofs < sizeof(struct ntfs_record_header) || (rh->usa_ofs & 1) != 0) {
      return -1;
   }// if
   if (rh->usa_count == 0) {
      return -1;
   }// if

   uint32_t sectors = size / 512;
   if ((uint32_t)rh->usa_count < 1 + sectors) {
      return -1;
   }// if

   uint32_t usa_bytes = (uint32_t)rh->usa_count * 2;
   if ((uint32_t)rh->usa_ofs + usa_bytes > 510) {
      return -1;
   }// if
   if ((uint32_t)rh->usa_ofs + usa_bytes > size) {
      return -1;
   }// if

   uint16_t usn = ntfs_get_u16((const uint8_t *)buf + rh->usa_ofs);

   for (uint32_t i = 0; i < sectors; i++) {
      uint8_t *sector_end = (uint8_t *)buf + ((i + 1) * 512) - 2;
      if (ntfs_get_u16(sector_end) != usn) {
         errorn("ntfs: USA fixup sequence mismatch");
         return -1;
      }// if
      uint16_t fixup_val = ntfs_get_u16((const uint8_t *)buf + rh->usa_ofs + ((i + 1) * 2));
      ntfs_put_u16(sector_end, fixup_val);
   }// for

   return 0;
}// ntfs_apply_fixup

int ntfs_read_mft_record(uint32_t record_num, void *buf) {
   if (!ntfs_mounted || !buf) {
      return -1;
   }// if
   if (ntfs_mft_record_size < 512 || ntfs_mft_record_size > 4096 || (ntfs_mft_record_size % 512) != 0) {
      return -1;
   }// if
   if (ntfs_sectors_per_cluster == 0) {
      return -1;
   }// if

   uint32_t spm = ntfs_mft_record_size / 512;
   if (ntfs_mft_start_lcn > UINT64_MAX / ntfs_sectors_per_cluster) {
      return -1;
   }// if
   uint64_t mft_sec = ntfs_mft_start_lcn * ntfs_sectors_per_cluster;
   uint64_t rec_offset = (uint64_t)record_num * spm;
   if (UINT64_MAX - mft_sec < rec_offset) {
      return -1;
   }// if
   uint64_t target_sec = mft_sec + rec_offset;
   if (UINT64_MAX - target_sec < spm) {
      return -1;
   }// if
   if (ntfs_total_sectors > 0 && target_sec + spm > ntfs_total_sectors) {
      return -1;
   }// if

   for (uint32_t i = 0; i < spm; i++) {
      if (virtio_read_sector(target_sec + i, (uint8_t *)buf + (i * 512)) != 0) {
         errorn("ntfs: failed reading MFT sector");
         return -1;
      }// if
   }// for

   struct ntfs_record_header *rh = (struct ntfs_record_header *)buf;
   if (rh->magic != NTFS_RECORD_MAGIC) {
      errorn("ntfs: invalid MFT record magic");
      return -1;
   }// if

   if (ntfs_apply_fixup(buf, ntfs_mft_record_size) != 0) {
      return -1;
   }// if

   if (rh->bytes_in_use < sizeof(struct ntfs_record_header) || rh->bytes_in_use > ntfs_mft_record_size) {
      return -1;
   }// if
   if (rh->attrs_offset < sizeof(struct ntfs_record_header) || rh->attrs_offset >= rh->bytes_in_use) {
      return -1;
   }// if
   if ((rh->attrs_offset & 3) != 0) {
      return -1;
   }// if

   return 0;
}// ntfs_read_mft_record

static const struct ntfs_attr_header *ntfs_find_attribute(const void *mft_buf, uint32_t attr_type) {
   if (!mft_buf) {
      return NULL;
   }// if
   const struct ntfs_record_header *rh = (const struct ntfs_record_header *)mft_buf;
   if (rh->bytes_in_use < sizeof(struct ntfs_record_header) || rh->bytes_in_use > ntfs_mft_record_size) {
      return NULL;
   }// if
   if (rh->attrs_offset < sizeof(struct ntfs_record_header) || rh->attrs_offset >= rh->bytes_in_use) {
      return NULL;
   }// if
   if ((rh->attrs_offset & 3) != 0) {
      return NULL;
   }// if

   const uint8_t *cur = (const uint8_t *)mft_buf + rh->attrs_offset;
   const uint8_t *end = (const uint8_t *)mft_buf + rh->bytes_in_use;
   const struct ntfs_attr_header *fallback = NULL;
   uint32_t iter = 0;

   while ((size_t)(end - cur) >= 16) {
      if (++iter > 256) {
         break;
      }// if
      const struct ntfs_attr_header *attr = (const struct ntfs_attr_header *)cur;
      if (attr->type == NTFS_ATTR_END) {
         break;
      }// if
      if (attr->length < 16 || attr->length > (size_t)(end - cur) || (attr->length & 3) != 0) {
         break;
      }// if
      if (attr->non_resident == 0) {
         if (attr->length < 24) {
            break;
         }// if
         if (attr->res.value_offset < 24 || attr->res.value_offset > attr->length) {
            break;
         }// if
         if (attr->res.value_length > attr->length - attr->res.value_offset) {
            break;
         }// if
      }// if
      else if (attr->non_resident == 1) {
         if (attr->length < 64) {
            break;
         }// if
         if (attr->non_res.mapping_pairs_offset < 64 || attr->non_res.mapping_pairs_offset > attr->length) {
            break;
         }// if
         if (attr->non_res.lowest_vcn > attr->non_res.highest_vcn && attr->non_res.highest_vcn != 0) {
            break;
         }// if
      }// else
      else {
         break;
      }// else

      if (attr->name_length > 0) {
         if (attr->name_offset < 16 || attr->name_offset > attr->length) {
            break;
         }// if
         if ((size_t)attr->name_length * 2 > (size_t)(attr->length - attr->name_offset)) {
            break;
         }// if
      }// if

      if (attr->type == attr_type) {
         if (attr_type == NTFS_ATTR_DATA) {
            if (attr->name_length == 0) {
               return attr;
            }// if
            if (!fallback) {
               fallback = attr;
            }// if
         }// if
         else {
            return attr;
         }// else
      }// if

      cur += attr->length;
   }// while

   return fallback;
}// ntfs_find_attribute

static int ntfs_read_non_resident_run(const uint8_t *run_ptr, const uint8_t *run_end, uint64_t target_vcn, uint64_t *out_lcn, uint64_t *out_len) {
   if (!run_ptr || !run_end || !out_lcn || !out_len || run_ptr >= run_end) {
      return -1;
   }// if

   uint64_t current_vcn = 0;
   int64_t current_lcn = 0;
   uint32_t run_count = 0;

   while (run_ptr < run_end && *run_ptr != 0) {
      if (++run_count > 65536) {
         return -1;
      }// if

      uint8_t len_bytes = *run_ptr & 0x0F;
      uint8_t off_bytes = (*run_ptr >> 4) & 0x0F;
      run_ptr++;

      if (len_bytes == 0 || len_bytes > 8 || off_bytes > 8) {
         return -1;
      }// if
      if ((size_t)(run_end - run_ptr) < (size_t)len_bytes + (size_t)off_bytes) {
         return -1;
      }// if

      uint64_t run_len = 0;
      for (int i = 0; i < len_bytes; i++) {
         run_len |= ((uint64_t)*run_ptr++) << (i * 8);
      }// for

      if (run_len == 0) {
         return -1;
      }// if
      if (UINT64_MAX - current_vcn < run_len) {
         return -1;
      }// if

      if (off_bytes == 0) {
         if (target_vcn >= current_vcn && target_vcn < current_vcn + run_len) {
            *out_lcn = 0;
            *out_len = run_len - (target_vcn - current_vcn);
            return 0;
         }// if
      }// if
      else {
         int64_t lcn_delta = 0;
         for (int i = 0; i < off_bytes; i++) {
            lcn_delta |= ((int64_t)*run_ptr++) << (i * 8);
         }// for
         if (off_bytes < 8 && ((run_ptr[-1] & 0x80) != 0)) {
            for (int i = off_bytes; i < 8; i++) {
               lcn_delta |= ((int64_t)0xFF) << (i * 8);
            }// for
         }// if

         if (lcn_delta > 0 && current_lcn > INT64_MAX - lcn_delta) {
            return -1;
         }// if
         if (lcn_delta < 0 && current_lcn < INT64_MIN - lcn_delta) {
            return -1;
         }// if

         current_lcn += lcn_delta;
         if (current_lcn < 0) {
            return -1;
         }// if

         if (target_vcn >= current_vcn && target_vcn < current_vcn + run_len) {
            uint64_t vcn_off = target_vcn - current_vcn;
            if ((uint64_t)current_lcn > UINT64_MAX - vcn_off) {
               return -1;
            }// if
            uint64_t res_lcn = (uint64_t)current_lcn + vcn_off;
            if (ntfs_sectors_per_cluster > 0 && res_lcn > UINT64_MAX / ntfs_sectors_per_cluster) {
               return -1;
            }// if
            if (ntfs_total_sectors > 0 && res_lcn * ntfs_sectors_per_cluster >= ntfs_total_sectors) {
               return -1;
            }// if

            *out_lcn = res_lcn;
            *out_len = run_len - vcn_off;
            return 0;
         }// if
      }// else

      current_vcn += run_len;
   }// while

   return -1;
}// ntfs_read_non_resident_run

static bool ntfs_match_name(const void *name_utf16, size_t len, const char *ascii_name) {
   if (!name_utf16 || !ascii_name) {
      return false;
   }// if
   if (len > NTFS_MAX_PATH_LEN) {
      return false;
   }// if
   size_t ascii_len = 0;
   while (ascii_name[ascii_len] != '\0') {
      if (++ascii_len > NTFS_MAX_PATH_LEN) {
         return false;
      }// if
   }// while
   if (ascii_len != len) {
      return false;
   }// if
   const uint8_t *p = (const uint8_t *)name_utf16;
   for (size_t i = 0; i < len; i++) {
      uint16_t wc = (uint16_t)p[i * 2] | ((uint16_t)p[i * 2 + 1] << 8);
      char c1 = (wc < 0x80) ? (char)wc : '?';
      char c2 = ascii_name[i];
      if (c1 >= 'A' && c1 <= 'Z') {
         c1 = (char)(c1 + ('a' - 'A'));
      }// if
      if (c2 >= 'A' && c2 <= 'Z') {
         c2 = (char)(c2 + ('a' - 'A'));
      }// if
      if (c1 != c2) {
         return false;
      }// if
   }// for
   return true;
}// ntfs_match_name

int ntfs_lookup_dir(uint32_t dir_record, const char *name, uint32_t *out_record) {
   if (!ntfs_mounted || !name || !out_record) {
      return -1;
   }// if

   if (name[0] == '.' && name[1] == '\0') {
      *out_record = dir_record;
      return 0;
   }// if

   static uint8_t dir_mft_buf[4096] __attribute__((aligned(8)));
   if (ntfs_read_mft_record(dir_record, dir_mft_buf) != 0) {
      return -1;
   }// if

   if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
      if (dir_record == NTFS_ROOT_INO) {
         *out_record = NTFS_ROOT_INO;
         return 0;
      }// if
      const struct ntfs_attr_header *fn_attr = ntfs_find_attribute(dir_mft_buf, NTFS_ATTR_FILE_NAME);
      if (fn_attr && !fn_attr->non_resident && fn_attr->length >= 24) {
         if (fn_attr->res.value_offset <= fn_attr->length && fn_attr->res.value_length <= fn_attr->length - fn_attr->res.value_offset) {
            if (fn_attr->res.value_length >= sizeof(struct ntfs_file_name_attr)) {
               const struct ntfs_file_name_attr *fn = (const struct ntfs_file_name_attr *)((const uint8_t *)fn_attr + fn_attr->res.value_offset);
               uint32_t parent = (uint32_t)(fn->parent_directory & 0x0000FFFFFFFFULL);
               if (parent != 0) {
                  *out_record = parent;
                  return 0;
               }// if
            }// if
         }// if
      }// if
      *out_record = NTFS_ROOT_INO;
      return 0;
   }// if

   const struct ntfs_attr_header *ir_attr = ntfs_find_attribute(dir_mft_buf, NTFS_ATTR_INDEX_ROOT);
   if (!ir_attr || ir_attr->non_resident || ir_attr->length < 24) {
      return -1;
   }// if

   if (ir_attr->res.value_offset > ir_attr->length || ir_attr->res.value_length > ir_attr->length - ir_attr->res.value_offset) {
      return -1;
   }// if

   uint32_t val_len = ir_attr->res.value_length;
   if (val_len < sizeof(struct ntfs_index_root_attr)) {
      return -1;
   }// if

   const struct ntfs_index_root_attr *ir = (const struct ntfs_index_root_attr *)((const uint8_t *)ir_attr + ir_attr->res.value_offset);
   const uint8_t *ih = (const uint8_t *)ir + 0x10;

   if (ir->entries_offset < 0x10 || ir->entries_offset > ir->total_size || ir->total_size > val_len - 0x10) {
      return -1;
   }// if
   if ((ir->entries_offset & 3) != 0 || (ir->total_size & 3) != 0) {
      return -1;
   }// if

   const uint8_t *entry_ptr = ih + ir->entries_offset;
   const uint8_t *entry_end = ih + ir->total_size;
   uint32_t max_entries = 1024;

   while (entry_ptr + sizeof(struct ntfs_index_entry) <= entry_end && max_entries-- > 0) {
      const struct ntfs_index_entry *ie = (const struct ntfs_index_entry *)entry_ptr;
      if (ie->length < sizeof(struct ntfs_index_entry) || (ie->length & 3) != 0) {
         break;
      }// if
      if (ie->length > (size_t)(entry_end - entry_ptr)) {
         break;
      }// if
      if (ie->flags & 0x02) {
         break;
      }// if

      if (ie->content_length >= 66 && ie->length >= sizeof(struct ntfs_index_entry) + 66) {
         const struct ntfs_file_name_attr *fn = (const struct ntfs_file_name_attr *)(entry_ptr + sizeof(struct ntfs_index_entry));
         size_t name_bytes = (size_t)fn->file_name_length * 2;
         if (name_bytes <= 512 && ie->content_length >= 66 + name_bytes && ie->length >= sizeof(struct ntfs_index_entry) + 66 + name_bytes) {
            if (ntfs_match_name(fn->file_name, fn->file_name_length, name)) {
               *out_record = (uint32_t)(ie->mft_reference & 0x0000FFFFFFFFULL);
               return 0;
            }// if
         }// if
      }// if

      entry_ptr += ie->length;
   }// while

   return -1;
}// ntfs_lookup_dir

int ntfs_resolve_path(const char *path, uint32_t *out_record) {
   if (!ntfs_mounted || !path || !out_record) {
      return -1;
   }// if
   while (*path == ' ') {
      path++;
   }// while
   if (path[0] != '/') {
      return -1;
   }// if
   while (*path == '/') {
      path++;
   }// while
   if (path[0] == '\0') {
      *out_record = NTFS_ROOT_INO;
      return 0;
   }// if

   char segment[NTFS_MAX_PATH_LEN];
   const char *p = path;
   uint32_t cur_record = NTFS_ROOT_INO;
   uint32_t depth = 0;

   while (*p != '\0') {
      size_t len = 0;
      while (*p != '/' && *p != '\0') {
         if (len >= sizeof(segment) - 1) {
            return -1;
         }// if
         segment[len++] = *p++;
      }// while
      segment[len] = '\0';
      while (*p == '/') {
         p++;
      }// while

      if (len == 0) {
         continue;
      }// if

      if (++depth > NTFS_MAX_PATH_DEPTH) {
         return -1;
      }// if

      uint32_t next_record = 0;
      if (ntfs_lookup_dir(cur_record, segment, &next_record) != 0) {
         return -1;
      }// if
      cur_record = next_record;
   }// while

   *out_record = cur_record;
   return 0;
}// ntfs_resolve_path

int ntfs_lookup(const char *path, uint32_t *out_record) {
   if (!ntfs_mounted || !path || !out_record) {
      return -1;
   }// if
   return ntfs_resolve_path(path, out_record);
}// ntfs_lookup

int ntfs_read(const char *path, void *buffer, uint32_t count, uint32_t offset) {
   if (!ntfs_mounted || !path || !buffer) {
      return -1;
   }// if
   if (count == 0) {
      return 0;
   }// if
   if (ntfs_bytes_per_cluster == 0 || ntfs_sectors_per_cluster == 0 || ntfs_bytes_per_cluster > NTFS_MAX_CLUSTER_SIZE) {
      return -1;
   }// if

   uint32_t record_num = 0;
   if (ntfs_resolve_path(path, &record_num) != 0) {
      return -1;
   }// if

   static uint8_t file_mft_buf[4096] __attribute__((aligned(8)));
   if (ntfs_read_mft_record(record_num, file_mft_buf) != 0) {
      return -1;
   }// if

   const struct ntfs_attr_header *data_attr = ntfs_find_attribute(file_mft_buf, NTFS_ATTR_DATA);
   if (!data_attr) {
      return -1;
   }// if

   if (!data_attr->non_resident) {
      if (data_attr->length < 24 || data_attr->res.value_offset > data_attr->length) {
         return -1;
      }// if
      uint32_t max_val = data_attr->length - data_attr->res.value_offset;
      uint32_t file_size = data_attr->res.value_length;
      if (file_size > max_val) {
         file_size = max_val;
      }// if
      if (offset >= file_size) {
         return 0;
      }// if
      uint32_t available = file_size - offset;
      uint32_t to_copy = count < available ? count : available;
      const uint8_t *val_ptr = (const uint8_t *)data_attr + data_attr->res.value_offset;
      memcpy(buffer, val_ptr + offset, to_copy);
      return (int)to_copy;
   }// if
   else {
      if (data_attr->length < 64) {
         return -1;
      }// if
      uint64_t file_size = data_attr->non_res.data_size;
      if (offset >= file_size) {
         return 0;
      }// if
      uint64_t available = file_size - offset;
      uint32_t to_read = (uint32_t)(count < available ? count : available);

      if (data_attr->non_res.mapping_pairs_offset < 64 || data_attr->non_res.mapping_pairs_offset >= data_attr->length) {
         return -1;
      }// if

      const uint8_t *run_ptr = (const uint8_t *)data_attr + data_attr->non_res.mapping_pairs_offset;
      const uint8_t *run_end = (const uint8_t *)data_attr + data_attr->length;
      uint32_t bytes_copied = 0;

      while (bytes_copied < to_read) {
         uint32_t cur_pos = offset + bytes_copied;
         uint64_t target_vcn = cur_pos / ntfs_bytes_per_cluster;
         uint32_t in_cluster_off = cur_pos % ntfs_bytes_per_cluster;

         uint64_t lcn = 0;
         uint64_t len = 0;
         if (ntfs_read_non_resident_run(run_ptr, run_end, target_vcn, &lcn, &len) != 0) {
            return -1;
         }// if

         if (lcn == 0) {
            memset(ntfs_cluster_buf, 0, ntfs_bytes_per_cluster);
         }// if
         else {
            if (lcn > UINT64_MAX / ntfs_sectors_per_cluster) {
               return -1;
            }// if
            uint64_t start_sec = lcn * ntfs_sectors_per_cluster;
            if (UINT64_MAX - start_sec < ntfs_sectors_per_cluster) {
               return -1;
            }// if
            if (ntfs_total_sectors > 0 && start_sec + ntfs_sectors_per_cluster > ntfs_total_sectors) {
               return -1;
            }// if
            for (uint32_t s = 0; s < ntfs_sectors_per_cluster; s++) {
               if (virtio_read_sector(start_sec + s, ntfs_cluster_buf + (s * 512)) != 0) {
                  return -1;
               }// if
            }// for
         }// else

         uint32_t chunk = ntfs_bytes_per_cluster - in_cluster_off;
         if (to_read - bytes_copied < chunk) {
            chunk = to_read - bytes_copied;
         }// if

         memcpy((uint8_t *)buffer + bytes_copied, ntfs_cluster_buf + in_cluster_off, chunk);
         bytes_copied += chunk;
      }// while

      return (int)bytes_copied;
   }// else
}// ntfs_read

int ntfs_list_dir(const char *path) {
   if (!ntfs_mounted || !path) {
      return -1;
   }// if

   uint32_t dir_record = 0;
   if (ntfs_resolve_path(path, &dir_record) != 0) {
      return -1;
   }// if

   static uint8_t list_mft_buf[4096] __attribute__((aligned(8)));
   if (ntfs_read_mft_record(dir_record, list_mft_buf) != 0) {
      return -1;
   }// if

   const struct ntfs_attr_header *ir_attr = ntfs_find_attribute(list_mft_buf, NTFS_ATTR_INDEX_ROOT);
   if (!ir_attr || ir_attr->non_resident || ir_attr->length < 24) {
      return -1;
   }// if

   if (ir_attr->res.value_offset > ir_attr->length || ir_attr->res.value_length > ir_attr->length - ir_attr->res.value_offset) {
      return -1;
   }// if

   uint32_t val_len = ir_attr->res.value_length;
   if (val_len < sizeof(struct ntfs_index_root_attr)) {
      return -1;
   }// if

   infon("ntfs: directory contents:");
   const struct ntfs_index_root_attr *ir = (const struct ntfs_index_root_attr *)((const uint8_t *)ir_attr + ir_attr->res.value_offset);
   const uint8_t *ih = (const uint8_t *)ir + 0x10;

   if (ir->entries_offset < 0x10 || ir->entries_offset > ir->total_size || ir->total_size > val_len - 0x10) {
      return -1;
   }// if
   if ((ir->entries_offset & 3) != 0 || (ir->total_size & 3) != 0) {
      return -1;
   }// if

   const uint8_t *entry_ptr = ih + ir->entries_offset;
   const uint8_t *entry_end = ih + ir->total_size;
   uint32_t max_entries = 1024;

   while (entry_ptr + sizeof(struct ntfs_index_entry) <= entry_end && max_entries-- > 0) {
      const struct ntfs_index_entry *ie = (const struct ntfs_index_entry *)entry_ptr;
      if (ie->length < sizeof(struct ntfs_index_entry) || (ie->length & 3) != 0) {
         break;
      }// if
      if (ie->length > (size_t)(entry_end - entry_ptr)) {
         break;
      }// if
      if (ie->flags & 0x02) {
         break;
      }// if

      if (ie->content_length >= 66 && ie->length >= sizeof(struct ntfs_index_entry) + 66) {
         const struct ntfs_file_name_attr *fn = (const struct ntfs_file_name_attr *)(entry_ptr + sizeof(struct ntfs_index_entry));
         size_t name_bytes = (size_t)fn->file_name_length * 2;
         if (name_bytes <= 512 && ie->content_length >= 66 + name_bytes && ie->length >= sizeof(struct ntfs_index_entry) + 66 + name_bytes) {
            char name_buf[NTFS_MAX_PATH_LEN];
            size_t nlen = fn->file_name_length < sizeof(name_buf) - 1 ? fn->file_name_length : sizeof(name_buf) - 1;
            const uint8_t *p = (const uint8_t *)fn->file_name;
            for (size_t i = 0; i < nlen; i++) {
               uint16_t wc = (uint16_t)p[i * 2] | ((uint16_t)p[i * 2 + 1] << 8);
               name_buf[i] = (wc < 0x80) ? (char)wc : '?';
            }// for
            name_buf[nlen] = '\0';

            print_character("  ");
            print_character(name_buf);
            print_character("\n");
         }// if
      }// if

      entry_ptr += ie->length;
   }// while

   return 0;
}// ntfs_list_dir

bool ntfs_is_valid_boot(const void *boot_buf) {
   if (!boot_buf) {
      return false;
   }// if
   const struct ntfs_boot_sector *bs = (const struct ntfs_boot_sector *)boot_buf;
   if (memcmp(bs->oem_id, NTFS_OEM_ID, 8) != 0) {
      return false;
   }// if
   const uint8_t *b = (const uint8_t *)boot_buf;
   if (b[510] != 0x55 || b[511] != 0xAA) {
      return false;
   }// if
   return true;
}// ntfs_is_valid_boot

void init_ntfs(void) {
   ntfs_mounted = false;
   static uint8_t sector_buf[512] __attribute__((aligned(8)));
   infon("ntfs: probing boot sector 0");

   if (virtio_read_sector(0, sector_buf) != 0) {
      errorn("ntfs: failed reading sector 0");
      return;
   }// if

   struct ntfs_boot_sector *bs = (struct ntfs_boot_sector *)sector_buf;
   if (!ntfs_is_valid_boot(bs)) {
      errorn("ntfs: signature mismatch");
      return;
   }// if

   if (bs->bytes_per_sector != 512 || bs->sectors_per_cluster == 0) {
      errorn("ntfs: unsupported sector or cluster size");
      return;
   }// if

   if ((bs->sectors_per_cluster & (bs->sectors_per_cluster - 1)) != 0 || bs->sectors_per_cluster > 128) {
      errorn("ntfs: invalid sectors per cluster");
      return;
   }// if

   ntfs_bytes_per_sector = bs->bytes_per_sector;
   ntfs_sectors_per_cluster = bs->sectors_per_cluster;
   ntfs_bytes_per_cluster = (uint32_t)bs->bytes_per_sector * bs->sectors_per_cluster;
   if (ntfs_bytes_per_cluster == 0 || ntfs_bytes_per_cluster > NTFS_MAX_CLUSTER_SIZE) {
      errorn("ntfs: invalid cluster size");
      return;
   }// if

   ntfs_total_sectors = bs->total_sectors;
   ntfs_mft_start_lcn = bs->mft_lcn;

   if (ntfs_total_sectors > 0 && bs->mft_lcn > ntfs_total_sectors / bs->sectors_per_cluster) {
      errorn("ntfs: mft lcn out of bounds");
      return;
   }// if

   if (bs->clusters_per_mft_record < 0) {
      if (-bs->clusters_per_mft_record < 9 || -bs->clusters_per_mft_record > 12) {
         errorn("ntfs: invalid mft record size");
         return;
      }// if
      ntfs_mft_record_size = 1U << (-bs->clusters_per_mft_record);
   }// if
   else if (bs->clusters_per_mft_record > 0) {
      uint64_t sz = (uint64_t)bs->clusters_per_mft_record * ntfs_bytes_per_cluster;
      if (sz < 512 || sz > 4096 || (sz % 512) != 0) {
         errorn("ntfs: unsupported mft record size");
         return;
      }// if
      ntfs_mft_record_size = (uint32_t)sz;
   }// else
   else {
      errorn("ntfs: invalid mft record size");
      return;
   }// else

   if (ntfs_mft_record_size < 512 || (ntfs_mft_record_size % 512) != 0 || ntfs_mft_record_size > 4096) {
      errorn("ntfs: unsupported mft record size");
      return;
   }// if

   ntfs_mounted = true;
   successn("ntfs: valid boot sector detected, filesystem mounted");
}// init_ntfs

bool ntfs_is_mounted(void) {
   return ntfs_mounted;
}// ntfs_is_mounted
