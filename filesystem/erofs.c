#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../include/kernel.h"
#include "../include/gid.h"
#include "../include/drivers.h"

#define EROFS_SUPER_MAGIC_V1 0xE0F5E1E2
#define EROFS_SUPER_OFFSET 1024
#define EROFS_INODE_LAYOUT_COMPACT 0
#define EROFS_INODE_LAYOUT_EXTENDED 1
#define EROFS_INODE_FLAT_PLAIN 0
#define EROFS_INODE_FLAT_COMPRESSION_LEGACY 1
#define EROFS_INODE_FLAT_INLINE 2
#define EROFS_INODE_FLAT_COMPRESSION 3
#define EROFS_FT_UNKNOWN 0
#define EROFS_FT_REG_FILE 1
#define EROFS_FT_DIR 2
#define EROFS_FT_CHRDEV 3
#define EROFS_FT_BLKDEV 4
#define EROFS_FT_FIFO 5
#define EROFS_FT_SOCK 6
#define EROFS_FT_SYMLINK 7
#define EROFS_MAX_PATH_LEN 256
#define EROFS_NAME_LEN 255
#define EROFS_MAX_PATH_DEPTH 64
#define EROFS_MAX_DIR_BLOCKS 2048

struct erofs_super_block {
   uint32_t magic;
   uint32_t checksum;
   uint32_t feature_compat;
   uint8_t blkszbits;
   uint8_t sb_extslots;
   uint16_t root_nid;
   uint64_t inos;
   uint64_t build_time;
   uint32_t build_time_nsec;
   uint32_t blocks;
   uint32_t meta_blkaddr;
   uint32_t xattr_blkaddr;
   uint8_t uuid[16];
   uint8_t volume_name[16];
   uint32_t feature_incompat;
   uint16_t extra_devices;
   uint16_t devt_slotoff;
   uint8_t reserved[38];
} __attribute__((packed));

struct erofs_inode_compact {
   uint16_t i_format;
   uint16_t i_xattr_icount;
   uint16_t i_mode;
   uint16_t i_nlink;
   uint32_t i_size;
   uint32_t i_reserved;
   union {
      uint32_t i_blkaddr;
      uint32_t i_blocks;
   } i_u;
   uint32_t i_ino;
   uint16_t i_uid;
   uint16_t i_gid;
   uint32_t i_checksum;
} __attribute__((packed));

struct erofs_inode_extended {
   uint16_t i_format;
   uint16_t i_xattr_icount;
   uint16_t i_mode;
   uint16_t i_reserved;
   uint64_t i_size;
   union {
      uint32_t i_blkaddr;
      uint32_t i_blocks;
   } i_u;
   uint32_t i_ino;
   uint32_t i_uid;
   uint32_t i_gid;
   uint64_t i_mtime;
   uint32_t i_mtime_nsec;
   uint32_t i_nlink;
   uint8_t reserved2[16];
} __attribute__((packed));

struct erofs_dirent {
   uint64_t nid;
   uint16_t nameoff;
   uint8_t file_type;
   uint8_t reserved;
} __attribute__((packed));

struct erofs_parsed_inode {
   uint64_t nid;
   uint16_t format;
   uint16_t mode;
   uint32_t size;
   uint32_t blkaddr;
   uint32_t inline_offset;
};

static bool erofs_mounted = false;
static uint32_t erofs_block_size = 4096;
static uint32_t erofs_total_blocks = 0;
static uint32_t erofs_meta_blkaddr = 0;
static uint64_t erofs_root_nid = 36;

extern int virtio_read_sector(uint64_t sector, void *buffer);

static int erofs_read_block(uint32_t blkaddr, void *buffer) {
   if (!buffer || erofs_block_size < 512 || (erofs_block_size & 511) != 0) {
      return -1;
   }// if
   if (erofs_total_blocks > 0 && blkaddr >= erofs_total_blocks) {
      return -1;
   }// if
   uint32_t sectors_per_block = erofs_block_size / 512;
   if (sectors_per_block == 0) {
      return -1;
   }// if
   if ((uint64_t)blkaddr > (0xFFFFFFFFFFFFFFFFULL / sectors_per_block)) {
      return -1;
   }// if
   uint64_t start_sector = (uint64_t)blkaddr * sectors_per_block;

   for (uint32_t i = 0; i < sectors_per_block; i++) {
      if (start_sector + i < start_sector) {
         return -1;
      }// if
      if (virtio_read_sector(start_sector + i, (uint8_t *)buffer + (i * 512)) != 0) {
         errorn("erofs: block read failure");
         return -1;
      }// if
   }// for
   return 0;
}// erofs_read_block

static int erofs_read_inode(uint64_t nid, struct erofs_parsed_inode *out) {
   if (!out || erofs_block_size < 512 || erofs_block_size > 4096) {
      return -1;
   }// if
   if (nid > (0xFFFFFFFFFFFFFFFFULL / 32)) {
      return -1;
   }// if
   uint64_t nid_offset = nid * 32;
   uint64_t meta_offset = (uint64_t)erofs_meta_blkaddr * erofs_block_size;
   if (0xFFFFFFFFFFFFFFFFULL - meta_offset < nid_offset) {
      return -1;
   }// if
   uint64_t byte_offset = meta_offset + nid_offset;
   uint64_t blkaddr_64 = byte_offset / erofs_block_size;
   if (blkaddr_64 > 0xFFFFFFFFULL) {
      return -1;
   }// if
   uint32_t blkaddr = (uint32_t)blkaddr_64;
   if (erofs_total_blocks > 0 && blkaddr >= erofs_total_blocks) {
      return -1;
   }// if
   uint32_t in_blk_offset = (uint32_t)(byte_offset % erofs_block_size);
   if (in_blk_offset + sizeof(struct erofs_inode_compact) > erofs_block_size) {
      return -1;
   }// if

   static __attribute__((aligned(8))) uint8_t blk_buf[4096];
   if (erofs_read_block(blkaddr, blk_buf) != 0) {
      return -1;
   }// if

   uint16_t i_format = 0;
   memcpy(&i_format, blk_buf + in_blk_offset, sizeof(uint16_t));
   uint16_t version = i_format & 1;
   uint16_t layout = (i_format >> 1) & 0x07;
   if (layout == 0 && (i_format & 0x07) == EROFS_INODE_FLAT_INLINE) {
      layout = EROFS_INODE_FLAT_INLINE;
   }// if

   out->nid = nid;
   out->format = layout;

   if (version == EROFS_INODE_LAYOUT_EXTENDED) {
      struct erofs_inode_extended die_buf;
      if (in_blk_offset + sizeof(struct erofs_inode_extended) <= erofs_block_size) {
         memcpy(&die_buf, blk_buf + in_blk_offset, sizeof(struct erofs_inode_extended));
      }// if
      else {
         if (blkaddr == 0xFFFFFFFF || (erofs_total_blocks > 0 && blkaddr + 1 >= erofs_total_blocks)) {
            return -1;
         }// if
         uint32_t first_part = erofs_block_size - in_blk_offset;
         if (first_part > sizeof(struct erofs_inode_extended)) {
            return -1;
         }// if
         memcpy(&die_buf, blk_buf + in_blk_offset, first_part);
         static __attribute__((aligned(8))) uint8_t next_buf[4096];
         if (erofs_read_block(blkaddr + 1, next_buf) != 0) {
            return -1;
         }// if
         memcpy((uint8_t *)&die_buf + first_part, next_buf, sizeof(struct erofs_inode_extended) - first_part);
      }// else
      out->mode = die_buf.i_mode;
      out->size = (die_buf.i_size > 0xFFFFFFFFULL) ? 0xFFFFFFFFU : (uint32_t)die_buf.i_size;
      out->blkaddr = die_buf.i_u.i_blkaddr;
      uint32_t inode_end = in_blk_offset + sizeof(struct erofs_inode_extended);
      uint32_t xattr_bytes = (uint32_t)die_buf.i_xattr_icount * 4;
      if (inode_end + xattr_bytes < inode_end || inode_end + xattr_bytes > erofs_block_size) {
         out->inline_offset = 0;
         if (layout == EROFS_INODE_FLAT_INLINE) {
            return -1;
         }// if
      }// if
      else {
         out->inline_offset = inode_end + xattr_bytes;
      }// else
   }// if
   else {
      struct erofs_inode_compact dic_buf;
      memcpy(&dic_buf, blk_buf + in_blk_offset, sizeof(struct erofs_inode_compact));
      out->mode = dic_buf.i_mode;
      out->size = dic_buf.i_size;
      out->blkaddr = dic_buf.i_u.i_blkaddr;
      uint32_t inode_end = in_blk_offset + sizeof(struct erofs_inode_compact);
      uint32_t xattr_bytes = (uint32_t)dic_buf.i_xattr_icount * 4;
      if (inode_end + xattr_bytes < inode_end || inode_end + xattr_bytes > erofs_block_size) {
         out->inline_offset = 0;
         if (layout == EROFS_INODE_FLAT_INLINE) {
            return -1;
         }// if
      }// if
      else {
         out->inline_offset = inode_end + xattr_bytes;
      }// else
   }// else
   return 0;
}// erofs_read_inode

static int erofs_lookup_dir(uint64_t dir_nid, const char *name, uint64_t *out_nid, uint8_t *out_type) {
   if (!name || *name == '\0') {
      return -1;
   }// if
   size_t target_len = strlen(name);
   if (target_len == 0 || target_len > EROFS_NAME_LEN) {
      return -1;
   }// if

   struct erofs_parsed_inode dir_ino;
   if (erofs_read_inode(dir_nid, &dir_ino) != 0) {
      return -1;
   }// if

   if ((dir_ino.mode & 0xF000) != 0x4000) {
      return -1;
   }// if

   if (dir_ino.size == 0) {
      return -1;
   }// if

   uint32_t num_blocks = 0;
   if (dir_ino.blkaddr != 0) {
      uint64_t calc_blocks = ((uint64_t)dir_ino.size + erofs_block_size - 1) / erofs_block_size;
      if (calc_blocks > EROFS_MAX_DIR_BLOCKS) {
         num_blocks = EROFS_MAX_DIR_BLOCKS;
      }// if
      else {
         num_blocks = (uint32_t)calc_blocks;
      }// else
   }// if
   else {
      num_blocks = 1;
      if (dir_ino.inline_offset >= erofs_block_size || dir_ino.size > erofs_block_size - dir_ino.inline_offset) {
         return -1;
      }// if
   }// else

   static __attribute__((aligned(8))) uint8_t dir_buf[4096];

   for (uint32_t b = 0; b < num_blocks; b++) {
      uint32_t blk;
      uint32_t base_off = 0;
      if (dir_ino.blkaddr != 0) {
         if (dir_ino.blkaddr > 0xFFFFFFFFU - b) {
            break;
         }// if
         blk = dir_ino.blkaddr + b;
         if (erofs_total_blocks > 0 && blk >= erofs_total_blocks) {
            break;
         }// if
      }// if
      else {
         uint64_t byte_offset = ((uint64_t)erofs_meta_blkaddr * erofs_block_size) + (dir_nid * 32);
         blk = (uint32_t)(byte_offset / erofs_block_size);
         base_off = dir_ino.inline_offset;
      }// else

      if (erofs_read_block(blk, dir_buf) != 0) {
         return -1;
      }// if

      if (base_off >= erofs_block_size || erofs_block_size - base_off < sizeof(struct erofs_dirent)) {
         continue;
      }// if

      uint32_t max_off = erofs_block_size - base_off;
      const struct erofs_dirent *de = (const struct erofs_dirent *)(dir_buf + base_off);
      uint16_t nameoff_first = de[0].nameoff;
      if (nameoff_first > max_off || nameoff_first < sizeof(struct erofs_dirent)) {
         continue;
      }// if

      uint32_t num_dirents = nameoff_first / sizeof(struct erofs_dirent);
      for (uint32_t i = 0; i < num_dirents; i++) {
         if (de[i].nid == 0) {
            continue;
         }// if

         uint16_t cur_off = de[i].nameoff;
         if (cur_off < nameoff_first || cur_off >= max_off) {
            continue;
         }// if

         uint32_t cur_name_len = 0;
         if (i + 1 < num_dirents) {
            uint16_t next_off = de[i + 1].nameoff;
            if (next_off <= cur_off || next_off > max_off) {
               continue;
            }// if
            cur_name_len = next_off - cur_off;
         }// if
         else {
            const char *str_start = (const char *)(dir_buf + base_off + cur_off);
            uint32_t max_str = max_off - cur_off;
            while (cur_name_len < max_str && cur_name_len < EROFS_NAME_LEN && str_start[cur_name_len] != '\0') {
               cur_name_len++;
            }// while
         }// else

         if (cur_name_len < 1 || cur_name_len > EROFS_NAME_LEN || cur_off + cur_name_len > max_off) {
            continue;
         }// if

         const char *cur_name = (const char *)(dir_buf + base_off + cur_off);
         uint32_t actual_len = cur_name_len;
         while (actual_len > 0 && cur_name[actual_len - 1] == '\0') {
            actual_len--;
         }// while

         if (actual_len == target_len && memcmp(cur_name, name, target_len) == 0) {
            if (out_nid) {
               *out_nid = de[i].nid;
            }// if
            if (out_type) {
               *out_type = de[i].file_type;
            }// if
            return 0;
         }// if
      }// for
   }// for
   return -1;
}// erofs_lookup_dir

static int erofs_resolve_path(const char *path, uint64_t *out_nid) {
   if (!path || path[0] != '/') {
      return -1;
   }// if

   while (*path == '/') {
      path++;
   }// while

   if (*path == '\0') {
      if (out_nid) {
         *out_nid = erofs_root_nid;
      }// if
      return 0;
   }// if

   char segment[EROFS_MAX_PATH_LEN];
   const char *p = path;
   uint64_t current_nid = erofs_root_nid;
   uint32_t depth = 0;

   while (*p != '\0') {
      if (++depth > EROFS_MAX_PATH_DEPTH) {
         return -1;
      }// if
      size_t len = 0;
      while (*p != '/' && *p != '\0' && len < sizeof(segment) - 1) {
         segment[len++] = *p++;
      }// while
      if (*p != '/' && *p != '\0') {
         return -1;
      }// if
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

      uint64_t next_nid = 0;
      uint8_t file_type = 0;
      if (erofs_lookup_dir(current_nid, segment, &next_nid, &file_type) != 0) {
         return -1;
      }// if
      if (*p != '\0' && file_type != EROFS_FT_DIR) {
         return -1;
      }// if
      current_nid = next_nid;
   }// while

   if (out_nid) {
      *out_nid = current_nid;
   }// if
   return 0;
}// erofs_resolve_path

int erofs_lookup(const char *path, uint64_t *out_nid) {
   if (!erofs_mounted || !path) {
      return -1;
   }// if
   return erofs_resolve_path(path, out_nid);
}// erofs_lookup

int erofs_read(const char *path, void *buffer, uint32_t count, uint32_t offset) {
   if (!erofs_mounted || !path || !buffer) {
      return -1;
   }// if
   if (count == 0) {
      return 0;
   }// if
   if (count > 0x7FFFFFFF) {
      count = 0x7FFFFFFF;
   }// if

   uint64_t nid = 0;
   if (erofs_resolve_path(path, &nid) != 0) {
      return -1;
   }// if

   struct erofs_parsed_inode ino;
   if (erofs_read_inode(nid, &ino) != 0) {
      return -1;
   }// if

   if ((ino.mode & 0xF000) == 0x4000) {
      return -1;
   }// if

   if (offset >= ino.size) {
      return 0;
   }// if
   uint32_t available = ino.size - offset;
   uint32_t to_read = (count < available) ? count : available;

   static __attribute__((aligned(8))) uint8_t blk_buf[4096];
   uint32_t bytes_copied = 0;

   while (bytes_copied < to_read) {
      uint32_t cur_pos = offset + bytes_copied;
      uint32_t blk_idx = cur_pos / erofs_block_size;
      uint32_t blk_offset = cur_pos % erofs_block_size;
      uint32_t chunk = erofs_block_size - blk_offset;
      if (to_read - bytes_copied < chunk) {
         chunk = to_read - bytes_copied;
      }// if

      if (ino.format == EROFS_INODE_FLAT_PLAIN) {
         if (ino.blkaddr > 0xFFFFFFFFU - blk_idx) {
            return -1;
         }// if
         uint32_t phys_blk = ino.blkaddr + blk_idx;
         if (erofs_read_block(phys_blk, blk_buf) != 0) {
            return -1;
         }// if
         memcpy((uint8_t *)buffer + bytes_copied, blk_buf + blk_offset, chunk);
      }// if
      else if (ino.format == EROFS_INODE_FLAT_INLINE) {
         uint32_t regular_blocks = ino.size / erofs_block_size;
         if (blk_idx < regular_blocks) {
            if (ino.blkaddr > 0xFFFFFFFFU - blk_idx) {
               return -1;
            }// if
            uint32_t phys_blk = ino.blkaddr + blk_idx;
            if (erofs_read_block(phys_blk, blk_buf) != 0) {
               return -1;
            }// if
            memcpy((uint8_t *)buffer + bytes_copied, blk_buf + blk_offset, chunk);
         }// if
         else {
            if (ino.inline_offset >= erofs_block_size) {
               return -1;
            }// if
            if (blk_offset > erofs_block_size - ino.inline_offset) {
               return -1;
            }// if
            if (chunk > (erofs_block_size - ino.inline_offset) - blk_offset) {
               return -1;
            }// if
            uint64_t byte_offset = ((uint64_t)erofs_meta_blkaddr * erofs_block_size) + (nid * 32);
            uint32_t ino_blk = (uint32_t)(byte_offset / erofs_block_size);
            if (erofs_read_block(ino_blk, blk_buf) != 0) {
               return -1;
            }// if
            memcpy((uint8_t *)buffer + bytes_copied, blk_buf + ino.inline_offset + blk_offset, chunk);
         }// else
      }// else if
      else {
         errorn("erofs: unsupported inode compression format");
         return -1;
      }// else

      bytes_copied += chunk;
   }// while

   return (int)bytes_copied;
}// erofs_read

int erofs_list_dir(const char *path) {
   if (!erofs_mounted || !path) {
      return -1;
   }// if

   uint64_t dir_nid = 0;
   if (erofs_resolve_path(path, &dir_nid) != 0) {
      return -1;
   }// if

   struct erofs_parsed_inode dir_ino;
   if (erofs_read_inode(dir_nid, &dir_ino) != 0) {
      return -1;
   }// if

   if ((dir_ino.mode & 0xF000) != 0x4000) {
      return -1;
   }// if

   if (dir_ino.size == 0) {
      return 0;
   }// if

   uint32_t num_blocks = 0;
   if (dir_ino.blkaddr != 0) {
      uint64_t calc_blocks = ((uint64_t)dir_ino.size + erofs_block_size - 1) / erofs_block_size;
      if (calc_blocks > EROFS_MAX_DIR_BLOCKS) {
         num_blocks = EROFS_MAX_DIR_BLOCKS;
      }// if
      else {
         num_blocks = (uint32_t)calc_blocks;
      }// else
   }// if
   else {
      num_blocks = 1;
      if (dir_ino.inline_offset >= erofs_block_size || dir_ino.size > erofs_block_size - dir_ino.inline_offset) {
         return -1;
      }// if
   }// else

   infon("erofs: directory contents:");
   static __attribute__((aligned(8))) uint8_t dir_buf[4096];

   for (uint32_t b = 0; b < num_blocks; b++) {
      uint32_t blk;
      uint32_t base_off = 0;
      if (dir_ino.blkaddr != 0) {
         if (dir_ino.blkaddr > 0xFFFFFFFFU - b) {
            break;
         }// if
         blk = dir_ino.blkaddr + b;
         if (erofs_total_blocks > 0 && blk >= erofs_total_blocks) {
            break;
         }// if
      }// if
      else {
         uint64_t byte_offset = ((uint64_t)erofs_meta_blkaddr * erofs_block_size) + (dir_nid * 32);
         blk = (uint32_t)(byte_offset / erofs_block_size);
         base_off = dir_ino.inline_offset;
      }// else

      if (erofs_read_block(blk, dir_buf) != 0) {
         return -1;
      }// if

      if (base_off >= erofs_block_size || erofs_block_size - base_off < sizeof(struct erofs_dirent)) {
         continue;
      }// if

      uint32_t max_off = erofs_block_size - base_off;
      const struct erofs_dirent *de = (const struct erofs_dirent *)(dir_buf + base_off);
      uint16_t nameoff_first = de[0].nameoff;
      if (nameoff_first > max_off || nameoff_first < sizeof(struct erofs_dirent)) {
         continue;
      }// if

      uint32_t num_dirents = nameoff_first / sizeof(struct erofs_dirent);
      for (uint32_t i = 0; i < num_dirents; i++) {
         if (de[i].nid == 0) {
            continue;
         }// if

         uint16_t cur_off = de[i].nameoff;
         if (cur_off < nameoff_first || cur_off >= max_off) {
            continue;
         }// if

         uint32_t cur_name_len = 0;
         if (i + 1 < num_dirents) {
            uint16_t next_off = de[i + 1].nameoff;
            if (next_off <= cur_off || next_off > max_off) {
               continue;
            }// if
            cur_name_len = next_off - cur_off;
         }// if
         else {
            const char *str_start = (const char *)(dir_buf + base_off + cur_off);
            uint32_t max_str = max_off - cur_off;
            while (cur_name_len < max_str && cur_name_len < EROFS_NAME_LEN && str_start[cur_name_len] != '\0') {
               cur_name_len++;
            }// while
         }// else

         if (cur_name_len < 1 || cur_name_len > EROFS_NAME_LEN || cur_off + cur_name_len > max_off) {
            continue;
         }// if

         const char *cur_name = (const char *)(dir_buf + base_off + cur_off);
         uint32_t actual_len = cur_name_len;
         while (actual_len > 0 && cur_name[actual_len - 1] == '\0') {
            actual_len--;
         }// while
         if (actual_len == 0 || actual_len > EROFS_NAME_LEN) {
            continue;
         }// if

         char name_buf[EROFS_MAX_PATH_LEN];
         if (actual_len >= sizeof(name_buf)) {
            actual_len = sizeof(name_buf) - 1;
         }// if
         memcpy(name_buf, cur_name, actual_len);
         name_buf[actual_len] = '\0';

         print_character("  ");
         print_character(name_buf);
         print_character("\n");
      }// for
   }// for
   return 0;
}// erofs_list_dir

void init_erofs(void) {
   static __attribute__((aligned(8))) uint8_t sb_buf[1024];
   infon("erofs: probing superblock at sector 2 (offset 1024)");

   if (virtio_read_sector(2, sb_buf) != 0 || virtio_read_sector(3, sb_buf + 512) != 0) {
      errorn("erofs: failed reading superblock sectors");
      return;
   }// if

   struct erofs_super_block *sb = (struct erofs_super_block *)sb_buf;
   if (sb->magic != EROFS_SUPER_MAGIC_V1) {
      errorn("erofs: signature mismatch");
      return;
   }// if

   if (sb->blkszbits < 9 || sb->blkszbits > 12) {
      errorn("erofs: invalid blkszbits");
      return;
   }// if

   uint32_t bsz = 1 << sb->blkszbits;
   if (bsz < 512 || bsz > 4096) {
      errorn("erofs: invalid block size");
      return;
   }// if

   if (sb->blocks == 0) {
      errorn("erofs: zero block count");
      return;
   }// if

   if (sb->meta_blkaddr >= sb->blocks) {
      errorn("erofs: invalid meta block address");
      return;
   }// if

   erofs_block_size = bsz;
   erofs_total_blocks = sb->blocks;
   erofs_meta_blkaddr = sb->meta_blkaddr;
   erofs_root_nid = sb->root_nid;
   erofs_mounted = true;

   successn("erofs: mounted high-performance read-only filesystem");
}// init_erofs

bool erofs_is_mounted(void) {
   return erofs_mounted;
}// erofs_is_mounted
