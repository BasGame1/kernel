#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../include/kernel.h"
#include "../include/gid.h"
#include "../include/drivers.h"

#define ISO_STANDARD_ID "CD001"
#define ISO_VD_PRIMARY 1
#define ISO_VD_END 255
#define ISO_SECTOR_SIZE 2048
#define ISO_MAX_PATH_LEN 256
#define ISO_MAX_PATH_DEPTH 64
#define ISO_FLAG_DIRECTORY 2
#define ISO_MAX_DIR_BLOCKS 4096
#define ISO_MIN_DATA_LBA 16

struct iso_volume_descriptor {
   uint8_t  type;
   char     id[5];
   uint8_t  version;
   uint8_t  data[2041];
} __attribute__((packed));

struct iso_directory_record {
   uint8_t  length;
   uint8_t  ext_attr_length;
   uint32_t extent_le;
   uint32_t extent_be;
   uint32_t size_le;
   uint32_t size_be;
   uint8_t  date[7];
   uint8_t  flags;
   uint8_t  file_unit_size;
   uint8_t  interleave_gap_size;
   uint16_t volume_seq_number_le;
   uint16_t volume_seq_number_be;
   uint8_t  name_len;
   char     name[];
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
   uint32_t path_table_size_le;
   uint32_t path_table_size_be;
   uint32_t type_l_path_table;
   uint32_t opt_type_l_path_table;
   uint32_t type_m_path_table;
   uint32_t opt_type_m_path_table;
   uint8_t  root_directory_record[34];
   char     volume_set_id[128];
   char     publisher_id[128];
   char     preparer_id[128];
   char     application_id[128];
   char     copyright_file_id[37];
   char     abstract_file_id[37];
   char     bibliographic_file_id[37];
   char     creation_date[17];
   char     modification_date[17];
   char     expiration_date[17];
   char     effective_date[17];
   uint8_t  file_structure_version;
   uint8_t  unused4;
   uint8_t  application_data[512];
   uint8_t  reserved[653];
} __attribute__((packed));

_Static_assert(sizeof(struct iso_primary_descriptor) == 2048, "iso_primary_descriptor size");
_Static_assert(offsetof(struct iso_primary_descriptor, root_directory_record) == 156, "root_directory_record offset");
_Static_assert(offsetof(struct iso_directory_record, name) == 33, "iso_directory_record name offset");

struct iso_entry_info {
   uint32_t extent_lba;
   uint32_t size;
   uint8_t  flags;
};

static bool iso9660_mounted = false;
static uint16_t iso_block_size = 2048;
static uint32_t iso_volume_blocks = 0;
static uint32_t iso_root_lba = 0;
static uint32_t iso_root_size = 0;

extern int virtio_read_sector(uint64_t sector, void *buffer);

void init_iso9660(void);
bool iso9660_is_mounted(void);
int iso9660_lookup(const char *path, uint32_t *out_lba);
int iso9660_lookup_dir(uint32_t dir_lba, const char *name, uint32_t *out_lba);
int iso9660_read(const char *path, void *buffer, uint32_t count, uint32_t offset);
int iso9660_list_dir(const char *path);

static inline uint16_t read_u16_le(const void *p) {
   const uint8_t *b = (const uint8_t *)p;
   return (uint16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
}// read_u16_le

static inline uint32_t read_u32_le(const void *p) {
   const uint8_t *b = (const uint8_t *)p;
   return (uint32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));
}// read_u32_le

static int iso9660_read_block(uint32_t lba, void *buffer) {
   if (!buffer) {
      return -1;
   }// if
   if (iso_volume_blocks > 0 && lba >= iso_volume_blocks) {
      return -1;
   }// if
   if (lba > 0x3FFFFFFFFFFFFFFFULL / 4) {
      return -1;
   }// if
   uint64_t start_sec = (uint64_t)lba * 4;
   for (int i = 0; i < 4; i++) {
      if (virtio_read_sector(start_sec + (uint64_t)i, (uint8_t *)buffer + (i * 512)) != 0) {
         memset(buffer, 0, 2048);
         return -1;
      }// if
   }// for
   return 0;
}// iso9660_read_block

static bool iso9660_name_match(const char *s1, const char *s2) {
   if (!s1 || !s2) {
      return false;
   }// if
   while (*s1 != '\0' && *s2 != '\0') {
      char c1 = *s1;
      char c2 = *s2;
      if (c1 >= 'A' && c1 <= 'Z') {
         c1 += ('a' - 'A');
      }// if
      if (c2 >= 'A' && c2 <= 'Z') {
         c2 += ('a' - 'A');
      }// if
      if (c1 != c2) {
         return false;
      }// if
      s1++;
      s2++;
   }// while
   return (*s1 == '\0' && *s2 == '\0');
}// iso9660_name_match

static void iso9660_normalize_name(const struct iso_directory_record *rec, char *out, size_t max_out) {
   if (!rec || !out || max_out == 0) {
      if (out && max_out > 0) {
         out[0] = '\0';
      }// if
      return;
   }// if
   out[0] = '\0';

   if (rec->length < sizeof(struct iso_directory_record)) {
      return;
   }// if

   size_t max_name = (size_t)rec->length - sizeof(struct iso_directory_record);
   if ((size_t)rec->name_len > max_name) {
      return;
   }// if

   if (rec->name_len == 1 && max_name >= 1 && ((uint8_t)rec->name[0] == 0)) {
      if (max_out > 1) {
         out[0] = '.';
         out[1] = '\0';
      }// if
      return;
   }// if
   if (rec->name_len == 1 && max_name >= 1 && ((uint8_t)rec->name[0] == 1)) {
      if (max_out > 2) {
         out[0] = '.';
         out[1] = '.';
         out[2] = '\0';
      }// if
      return;
   }// if

   size_t len = rec->name_len;
   if (len >= max_out) {
      len = max_out - 1;
   }// if
   memcpy(out, rec->name, len);
   out[len] = '\0';

   if (!(rec->flags & ISO_FLAG_DIRECTORY)) {
      for (size_t i = 0; i < len; i++) {
         if (out[i] == ';') {
            len = i;
            out[len] = '\0';
            break;
         }// if
      }// for
      if (len > 0 && out[len - 1] == '.') {
         len--;
         out[len] = '\0';
      }// if
   }// if
}// iso9660_normalize_name

static int iso9660_find_in_dir(uint32_t dir_lba, uint32_t dir_size, const char *name, struct iso_entry_info *out_info) {
   if (!iso9660_mounted || !name || dir_lba < ISO_MIN_DATA_LBA) {
      return -1;
   }// if
   if (iso_volume_blocks > 0 && dir_lba >= iso_volume_blocks) {
      return -1;
   }// if

   static uint8_t blk_buf[2048] __attribute__((aligned(8)));

   if (dir_size == 0) {
      if (iso9660_read_block(dir_lba, blk_buf) != 0) {
         return -1;
      }// if
      const struct iso_directory_record *self_rec = (const struct iso_directory_record *)blk_buf;
      if (self_rec->length >= sizeof(struct iso_directory_record)) {
         dir_size = read_u32_le(&self_rec->size_le);
      }// if
      if (dir_size == 0) {
         dir_size = 2048;
      }// if
   }// if

   if (iso_volume_blocks > 0) {
      uint64_t max_bytes = (uint64_t)iso_volume_blocks * 2048ULL;
      if ((uint64_t)dir_size > max_bytes) {
         dir_size = (uint32_t)max_bytes;
      }// if
   }// if

   uint32_t num_blocks = (dir_size / 2048) + ((dir_size % 2048 != 0) ? 1 : 0);
   if (num_blocks == 0) {
      num_blocks = 1;
   }// if
   if (num_blocks > ISO_MAX_DIR_BLOCKS) {
      num_blocks = ISO_MAX_DIR_BLOCKS;
   }// if
   if (iso_volume_blocks > 0 && num_blocks > iso_volume_blocks) {
      num_blocks = iso_volume_blocks;
   }// if

   for (uint32_t b = 0; b < num_blocks; b++) {
      if (dir_lba > 0xFFFFFFFFU - b) {
         break;
      }// if
      if (iso_volume_blocks > 0 && dir_lba + b >= iso_volume_blocks) {
         break;
      }// if
      if (iso9660_read_block(dir_lba + b, blk_buf) != 0) {
         return -1;
      }// if

      uint32_t offset = 0;
      while (offset <= 2048 - sizeof(struct iso_directory_record)) {
         const struct iso_directory_record *rec = (const struct iso_directory_record *)(blk_buf + offset);
         if (rec->length < sizeof(struct iso_directory_record)) {
            break;
         }// if
         if (offset > 2048 - rec->length) {
            break;
         }// if

         size_t max_nlen = (size_t)rec->length - sizeof(struct iso_directory_record);
         if ((size_t)rec->name_len <= max_nlen) {
            char norm_name[ISO_MAX_PATH_LEN];
            iso9660_normalize_name(rec, norm_name, sizeof(norm_name));
            if (iso9660_name_match(norm_name, name)) {
               if (out_info) {
                  out_info->extent_lba = read_u32_le(&rec->extent_le);
                  out_info->size = read_u32_le(&rec->size_le);
                  out_info->flags = rec->flags;
               }// if
               return 0;
            }// if
         }// if

         offset += rec->length;
      }// while
   }// for

   return -1;
}// iso9660_find_in_dir

int iso9660_lookup_dir(uint32_t dir_lba, const char *name, uint32_t *out_lba) {
   if (!iso9660_mounted || !name || dir_lba < ISO_MIN_DATA_LBA) {
      return -1;
   }// if
   if (iso_volume_blocks > 0 && dir_lba >= iso_volume_blocks) {
      return -1;
   }// if
   struct iso_entry_info info;
   if (iso9660_find_in_dir(dir_lba, 0, name, &info) != 0) {
      return -1;
   }// if
   if (out_lba) {
      memcpy(out_lba, &info.extent_lba, sizeof(uint32_t));
   }// if
   return 0;
}// iso9660_lookup_dir

static int iso9660_resolve_path(const char *path, struct iso_entry_info *out_info) {
   if (!iso9660_mounted || !path) {
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

   uint32_t cur_lba = iso_root_lba;
   uint32_t cur_size = iso_root_size;
   uint8_t cur_flags = ISO_FLAG_DIRECTORY;

   if (cur_lba < ISO_MIN_DATA_LBA || (iso_volume_blocks > 0 && cur_lba >= iso_volume_blocks)) {
      return -1;
   }// if

   if (out_info) {
      out_info->extent_lba = cur_lba;
      out_info->size = cur_size;
      out_info->flags = cur_flags;
   }// if

   if (path[0] == '\0') {
      return 0;
   }// if

   char segment[ISO_MAX_PATH_LEN];
   const char *p = path;
   uint32_t depth = 0;
   uint32_t visited_lbas[ISO_MAX_PATH_DEPTH + 1];
   uint32_t visited_count = 0;
   visited_lbas[visited_count++] = cur_lba;

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
      if (len == 1 && segment[0] == '.') {
         continue;
      }// if

      if (++depth > ISO_MAX_PATH_DEPTH) {
         return -1;
      }// if

      if (!(cur_flags & ISO_FLAG_DIRECTORY)) {
         return -1;
      }// if

      struct iso_entry_info next_info;
      if (iso9660_find_in_dir(cur_lba, cur_size, segment, &next_info) != 0) {
         return -1;
      }// if

      if (next_info.extent_lba < ISO_MIN_DATA_LBA) {
         return -1;
      }// if
      if (iso_volume_blocks > 0 && next_info.extent_lba >= iso_volume_blocks) {
         return -1;
      }// if

      if (next_info.flags & ISO_FLAG_DIRECTORY) {
         bool is_dotdot = (len == 2 && segment[0] == '.' && segment[1] == '.');
         if (is_dotdot) {
            if (visited_count > 1) {
               visited_count--;
            }// if
         }// if
         else {
            for (uint32_t v = 0; v < visited_count; v++) {
               if (visited_lbas[v] == next_info.extent_lba) {
                  return -1;
               }// if
            }// for
            if (visited_count < ISO_MAX_PATH_DEPTH) {
               visited_lbas[visited_count++] = next_info.extent_lba;
            }// if
         }// else
      }// if

      cur_lba = next_info.extent_lba;
      cur_size = next_info.size;
      cur_flags = next_info.flags;

      if (out_info) {
         out_info->extent_lba = cur_lba;
         out_info->size = cur_size;
         out_info->flags = cur_flags;
      }// if
   }// while

   return 0;
}// iso9660_resolve_path

int iso9660_lookup(const char *path, uint32_t *out_lba) {
   if (!iso9660_mounted || !path) {
      return -1;
   }// if
   struct iso_entry_info info;
   if (iso9660_resolve_path(path, &info) != 0) {
      return -1;
   }// if
   if (out_lba) {
      memcpy(out_lba, &info.extent_lba, sizeof(uint32_t));
   }// if
   return 0;
}// iso9660_lookup

int iso9660_read(const char *path, void *buffer, uint32_t count, uint32_t offset) {
   if (!iso9660_mounted || !path || !buffer) {
      return -1;
   }// if
   if (count == 0) {
      return 0;
   }// if
   if (count > 0x7FFFFFFF) {
      count = 0x7FFFFFFF;
   }// if

   struct iso_entry_info info;
   if (iso9660_resolve_path(path, &info) != 0) {
      return -1;
   }// if

   if (info.flags & ISO_FLAG_DIRECTORY) {
      return -1;
   }// if

   if (offset >= info.size) {
      return 0;
   }// if

   if (info.extent_lba < ISO_MIN_DATA_LBA) {
      return -1;
   }// if
   if (iso_volume_blocks > 0 && info.extent_lba >= iso_volume_blocks) {
      return -1;
   }// if

   uint32_t available = info.size - offset;
   uint32_t to_read = (count < available) ? count : available;
   if (to_read > 0x7FFFFFFF) {
      to_read = 0x7FFFFFFF;
   }// if

   if ((uintptr_t)buffer > (uintptr_t)(~0UL) - (uintptr_t)to_read) {
      return -1;
   }// if

   static uint8_t blk_buf[2048] __attribute__((aligned(8)));
   uint32_t bytes_copied = 0;
   uint32_t bsz = 2048;

   while (bytes_copied < to_read) {
      uint32_t cur_pos = offset + bytes_copied;
      uint32_t blk_idx = cur_pos / bsz;
      uint32_t blk_offset = cur_pos % bsz;
      uint32_t chunk = bsz - blk_offset;
      if (to_read - bytes_copied < chunk) {
         chunk = to_read - bytes_copied;
      }// if

      if (info.extent_lba > 0xFFFFFFFFU - blk_idx) {
         return -1;
      }// if

      uint32_t phys_blk = info.extent_lba + blk_idx;
      if (iso_volume_blocks > 0 && phys_blk >= iso_volume_blocks) {
         return -1;
      }// if

      if (iso9660_read_block(phys_blk, blk_buf) != 0) {
         return -1;
      }// if

      memcpy((uint8_t *)buffer + bytes_copied, blk_buf + blk_offset, chunk);
      bytes_copied += chunk;
   }// while

   return (int)bytes_copied;
}// iso9660_read

int iso9660_list_dir(const char *path) {
   if (!iso9660_mounted || !path) {
      return -1;
   }// if

   struct iso_entry_info info;
   if (iso9660_resolve_path(path, &info) != 0) {
      return -1;
   }// if

   if (!(info.flags & ISO_FLAG_DIRECTORY)) {
      return -1;
   }// if

   if (info.extent_lba < ISO_MIN_DATA_LBA) {
      return -1;
   }// if
   if (iso_volume_blocks > 0 && info.extent_lba >= iso_volume_blocks) {
      return -1;
   }// if

   infon("iso9660: directory contents:");

   static uint8_t dir_buf[2048] __attribute__((aligned(8)));
   uint32_t dir_size = info.size;
   if (iso_volume_blocks > 0) {
      uint64_t max_bytes = (uint64_t)iso_volume_blocks * 2048ULL;
      if ((uint64_t)dir_size > max_bytes) {
         dir_size = (uint32_t)max_bytes;
      }// if
   }// if

   uint32_t num_blocks = (dir_size / 2048) + ((dir_size % 2048 != 0) ? 1 : 0);
   if (num_blocks == 0) {
      num_blocks = 1;
   }// if
   if (num_blocks > ISO_MAX_DIR_BLOCKS) {
      num_blocks = ISO_MAX_DIR_BLOCKS;
   }// if
   if (iso_volume_blocks > 0 && num_blocks > iso_volume_blocks) {
      num_blocks = iso_volume_blocks;
   }// if

   for (uint32_t b = 0; b < num_blocks; b++) {
      if (info.extent_lba > 0xFFFFFFFFU - b) {
         break;
      }// if
      if (iso_volume_blocks > 0 && info.extent_lba + b >= iso_volume_blocks) {
         break;
      }// if
      if (iso9660_read_block(info.extent_lba + b, dir_buf) != 0) {
         return -1;
      }// if

      uint32_t offset = 0;
      while (offset <= 2048 - sizeof(struct iso_directory_record)) {
         const struct iso_directory_record *rec = (const struct iso_directory_record *)(dir_buf + offset);
         if (rec->length < sizeof(struct iso_directory_record)) {
            break;
         }// if
         if (offset > 2048 - rec->length) {
            break;
         }// if

         size_t max_nlen = (size_t)rec->length - sizeof(struct iso_directory_record);
         if ((size_t)rec->name_len <= max_nlen) {
            char name_buf[ISO_MAX_PATH_LEN];
            iso9660_normalize_name(rec, name_buf, sizeof(name_buf));
            if (name_buf[0] != '\0') {
               print_character("  ");
               print_character(name_buf);
               print_character("\n");
            }// if
         }// if

         offset += rec->length;
      }// while
   }// for

   return 0;
}// iso9660_list_dir

void init_iso9660(void) {
   static uint8_t sec_buf[2048] __attribute__((aligned(8)));
   infon("Probing ISO9660 CD-ROM filesystem (sector 16)");

   iso9660_mounted = false;
   iso_volume_blocks = 0;
   iso_block_size = 2048;
   iso_root_lba = 0;
   iso_root_size = 0;

   if (iso9660_read_block(16, sec_buf) != 0) {
      errorn("iso9660: failed reading volume descriptor sector");
      return;
   }// if

   struct iso_primary_descriptor *pvd = (struct iso_primary_descriptor *)sec_buf;
   bool match = true;
   for (int i = 0; i < 5; i++) {
      if (pvd->id[i] != ISO_STANDARD_ID[i]) {
         match = false;
         break;
      }// if
   }// for

   if (!match || pvd->type != ISO_VD_PRIMARY || pvd->version != 1) {
      errorn("iso9660: not detected");
      return;
   }// if

   uint16_t blk_size = read_u16_le(&pvd->logical_block_size_le);
   if (blk_size != 2048 && blk_size != 0) {
      errorn("iso9660: unsupported block size");
      return;
   }// if

   uint32_t vol_blocks = read_u32_le(&pvd->volume_space_size_le);
   if (vol_blocks <= 16) {
      errorn("iso9660: invalid volume size");
      return;
   }// if

   const struct iso_directory_record *root_dr = (const struct iso_directory_record *)pvd->root_directory_record;
   if (root_dr->length < 34) {
      errorn("iso9660: invalid root directory record");
      return;
   }// if

   if (!(root_dr->flags & ISO_FLAG_DIRECTORY)) {
      errorn("iso9660: root is not a directory");
      return;
   }// if

   uint32_t root_lba = read_u32_le(&root_dr->extent_le);
   uint32_t root_size = read_u32_le(&root_dr->size_le);

   if (root_lba < ISO_MIN_DATA_LBA || root_lba >= vol_blocks) {
      errorn("iso9660: invalid root directory extent");
      return;
   }// if

   if (root_size == 0 || (uint64_t)root_size > (uint64_t)vol_blocks * 2048ULL) {
      errorn("iso9660: invalid root directory size");
      return;
   }// if

   iso_volume_blocks = vol_blocks;
   iso_block_size = 2048;
   iso_root_lba = root_lba;
   iso_root_size = root_size;
   iso9660_mounted = true;
   successn("iso9660: mounted");
}// init_iso9660

bool iso9660_is_mounted(void) {
   return iso9660_mounted;
}// iso9660_is_mounted
