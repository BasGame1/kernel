#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "../include/hardware.h"
#include "../include/kernel.h"
#include "../include/userspace.h"
#include "../include/gid.h"

int ext2_virtio_transfer(uint32_t sector, void *buffer, bool is_write);

static uint32_t ext2_block_size = 1024;
static uint32_t ext2_inode_size = 128;
static uint32_t ext2_inodes_per_group = 0;
static uint32_t ext2_blocks_per_group = 0;
static uint32_t ext2_total_groups = 0;
static uint32_t ext2_first_data_block = 1;
static uint32_t ext2_inodes_count = 0;
static uint32_t ext2_blocks_count = 0;
static uint32_t ext2_first_ino = 11;
bool ext2_ready = false;
uintptr_t ext2_ramdisk_base = 0;
size_t ext2_ramdisk_size = 0;

static int ext2_read_block_safe(uint32_t block, void *buffer) {
   if (buffer == NULL || ext2_block_size < 512) {
      return -1;
   }// if
   uint32_t sectors_per_block = ext2_block_size / 512;
   if (sectors_per_block == 0 || block > 0xFFFFFFFF / sectors_per_block) {
      memset(buffer, 0, ext2_block_size);
      return -1;
   }// if
   uint32_t start_sector = block * sectors_per_block;
   for (uint32_t i = 0; i < sectors_per_block; i++) {
      if (ext2_virtio_transfer(start_sector + i, (uint8_t *)buffer + (i * 512), false) != 0) {
         error("Read failed at sector ");
         print_dec(start_sector + i);
         print_character("\n");
         memset(buffer, 0, ext2_block_size);
         return -1;
      }// if
   }// for
   return 0;
}// ext2_read_block_safe

static int ext2_write_block_safe(uint32_t block, const void *buffer) {
   if (buffer == NULL || ext2_block_size < 512) {
      return -1;
   }// if
   uint32_t sectors_per_block = ext2_block_size / 512;
   if (sectors_per_block == 0 || block > 0xFFFFFFFF / sectors_per_block) {
      return -1;
   }// if
   uint32_t start_sector = block * sectors_per_block;
   for (uint32_t i = 0; i < sectors_per_block; i++) {
      if (ext2_virtio_transfer(start_sector + i, (void *)((uintptr_t)buffer + (i * 512)), true) != 0) {
         error("Write failed at sector ");
         print_dec(start_sector + i);
         print_character("\n");
         return -1;
      }// if
   }// for
   return 0;
}// ext2_write_block_safe

void ext2_read_block(uint32_t block, void *buffer) {
   ext2_read_block_safe(block, buffer);
}// ext2_read_block

void ext2_write_block(uint32_t block, void *buffer) {
   ext2_write_block_safe(block, buffer);
}// ext2_write_block

static int ext2_get_group_desc(uint32_t group, struct ext2_group_desc *desc) {
   if (!ext2_ready || group >= ext2_total_groups || desc == NULL || ext2_block_size == 0) {
      return -1;
   }// if
   uint32_t bgd_block = (ext2_block_size == 1024) ? 2 : 1;
   uint32_t desc_size = sizeof(struct ext2_group_desc);
   if (desc_size == 0 || group > 0xFFFFFFFF / desc_size) {
      return -1;
   }// if
   uint32_t blk_offset = (group * desc_size) / ext2_block_size;
   uint32_t byte_offset = (group * desc_size) % ext2_block_size;
   if (byte_offset + desc_size > ext2_block_size) {
      return -1;
   }// if

   static uint8_t bgd_buffer[4096];
   if (ext2_read_block_safe(bgd_block + blk_offset, bgd_buffer) != 0) {
      return -1;
   }// if
   memcpy(desc, bgd_buffer + byte_offset, desc_size);
   return 0;
}// ext2_get_group_desc

static int ext2_put_group_desc(uint32_t group, const struct ext2_group_desc *desc) {
   if (!ext2_ready || group >= ext2_total_groups || desc == NULL || ext2_block_size == 0) {
      return -1;
   }// if
   uint32_t bgd_block = (ext2_block_size == 1024) ? 2 : 1;
   uint32_t desc_size = sizeof(struct ext2_group_desc);
   if (desc_size == 0 || group > 0xFFFFFFFF / desc_size) {
      return -1;
   }// if
   uint32_t blk_offset = (group * desc_size) / ext2_block_size;
   uint32_t byte_offset = (group * desc_size) % ext2_block_size;
   if (byte_offset + desc_size > ext2_block_size) {
      return -1;
   }// if

   static uint8_t bgd_buffer[4096];
   if (ext2_read_block_safe(bgd_block + blk_offset, bgd_buffer) != 0) {
      return -1;
   }// if
   memcpy(bgd_buffer + byte_offset, desc, desc_size);
   return ext2_write_block_safe(bgd_block + blk_offset, bgd_buffer);
}// ext2_put_group_desc

void init_ext2(void) {
   ext2_ready = false;
   static uint8_t buffer[4096];
   if (ext2_virtio_transfer(2, buffer, false) != 0 || ext2_virtio_transfer(3, buffer + 512, false) != 0) {
      errorn("Failed to read ext2 superblock");
      return;
   }// if
   struct ext2_super_block *sb = (struct ext2_super_block *)buffer;
   if (sb->ext2_magic != EXT2_MAGIC) {
      error("ext2 magic not found: ");
      print_hex(sb->ext2_magic);
      print_character("\n");
      return;
   }// if

   success("ext2 magic found: ");
   print_hex(sb->ext2_magic);
   print_character("\n");

   if (sb->ext2_log_block_size > 2) {
      errorn("ext2 block size unsupported (>4096)");
      return;
   }// if

   if (sb->ext2_blocks_count == 0 || sb->ext2_inodes_count == 0) {
      errorn("ext2 blocks or inodes count is zero");
      return;
   }// if

   if (sb->ext2_blocks_per_group == 0 || sb->ext2_inodes_per_group == 0) {
      errorn("ext2 blocks or inodes per group is zero");
      return;
   }// if

   ext2_block_size = 1024 << sb->ext2_log_block_size;
   ext2_inodes_per_group = sb->ext2_inodes_per_group;
   ext2_blocks_per_group = sb->ext2_blocks_per_group;
   ext2_first_data_block = sb->ext2_first_data_block;
   ext2_inodes_count = sb->ext2_inodes_count;
   ext2_blocks_count = sb->ext2_blocks_count;
   ext2_first_ino = (sb->ext2_rev_level >= 1 && sb->ext2_first_ino != 0) ? sb->ext2_first_ino : 11;
   ext2_inode_size = (sb->ext2_rev_level >= 1 && sb->ext2_inode_size != 0) ? sb->ext2_inode_size : 128;

   if (ext2_inode_size < 128 || ext2_inode_size > ext2_block_size || (ext2_block_size % ext2_inode_size) != 0) {
      errorn("ext2 invalid inode size");
      return;
   }// if

   ext2_total_groups = (ext2_blocks_count / ext2_blocks_per_group) + ((ext2_blocks_count % ext2_blocks_per_group) ? 1 : 0);
   if (ext2_total_groups == 0) {
      ext2_total_groups = 1;
   }// if
   ext2_ready = true;

   info("Ext2 block size: ");
   print_dec(ext2_block_size);
   print_character("\n");
}// init_ext2

uint32_t ext2_read_inode(uint32_t inode_no, struct ext2_inode *out) {
   if (inode_no == 0 || inode_no > ext2_inodes_count || out == NULL || !ext2_ready || ext2_inodes_per_group == 0) {
      return 1;
   }// if
   uint32_t group = (inode_no - 1) / ext2_inodes_per_group;
   uint32_t index = (inode_no - 1) % ext2_inodes_per_group;

   struct ext2_group_desc bgd;
   if (ext2_get_group_desc(group, &bgd) != 0) {
      return 1;
   }// if

   if (bgd.bg_inode_table < ext2_first_data_block || bgd.bg_inode_table >= ext2_blocks_count) {
      return 1;
   }// if

   if (ext2_inode_size == 0 || index > 0xFFFFFFFF / ext2_inode_size) {
      return 1;
   }// if
   uint32_t inode_block = bgd.bg_inode_table + (index * ext2_inode_size) / ext2_block_size;
   uint32_t byte_offset = (index * ext2_inode_size) % ext2_block_size;

   if (byte_offset + sizeof(struct ext2_inode) > ext2_block_size) {
      return 1;
   }// if

   static uint8_t buffer[4096];
   if (ext2_read_block_safe(inode_block, buffer) != 0) {
      return 1;
   }// if
   memcpy(out, buffer + byte_offset, sizeof(struct ext2_inode));
   return 0;
}// ext2_read_inode

uint32_t ext2_write_inode(uint32_t inode_no, const struct ext2_inode *in) {
   if (inode_no == 0 || inode_no > ext2_inodes_count || in == NULL || !ext2_ready || ext2_inodes_per_group == 0) {
      return 1;
   }// if
   uint32_t group = (inode_no - 1) / ext2_inodes_per_group;
   uint32_t index = (inode_no - 1) % ext2_inodes_per_group;

   struct ext2_group_desc bgd;
   if (ext2_get_group_desc(group, &bgd) != 0) {
      return 1;
   }// if

   if (bgd.bg_inode_table < ext2_first_data_block || bgd.bg_inode_table >= ext2_blocks_count) {
      return 1;
   }// if

   if (ext2_inode_size == 0 || index > 0xFFFFFFFF / ext2_inode_size) {
      return 1;
   }// if
   uint32_t inode_block = bgd.bg_inode_table + (index * ext2_inode_size) / ext2_block_size;
   uint32_t byte_offset = (index * ext2_inode_size) % ext2_block_size;

   if (byte_offset + sizeof(struct ext2_inode) > ext2_block_size) {
      return 1;
   }// if

   static uint8_t buffer[4096];
   if (ext2_read_block_safe(inode_block, buffer) != 0) {
      return 1;
   }// if
   memcpy(buffer + byte_offset, in, sizeof(struct ext2_inode));
   return (ext2_write_block_safe(inode_block, buffer) == 0) ? 0 : 1;
}// ext2_write_inode

static uint32_t ext2_get_inode_block(const struct ext2_inode *inode, uint32_t blk_idx) {
   if (inode == NULL || !ext2_ready || ext2_block_size == 0) {
      return 0;
   }// if
   if (blk_idx < 12) {
      uint32_t b = inode->inode_block[blk_idx];
      return (b >= ext2_first_data_block && b < ext2_blocks_count) ? b : 0;
   }// if
   blk_idx -= 12;
   uint32_t ptrs_per_blk = ext2_block_size / 4;
   if (ptrs_per_blk == 0) {
      return 0;
   }// if
   if (blk_idx < ptrs_per_blk) {
      uint32_t indir_blk = inode->inode_block[12];
      if (indir_blk < ext2_first_data_block || indir_blk >= ext2_blocks_count) {
         return 0;
      }// if
      static uint32_t indir_buf[1024];
      if (ext2_read_block_safe(indir_blk, indir_buf) != 0) {
         return 0;
      }// if
      uint32_t b = indir_buf[blk_idx];
      return (b >= ext2_first_data_block && b < ext2_blocks_count && b != indir_blk) ? b : 0;
   }// if
   blk_idx -= ptrs_per_blk;
   if (ptrs_per_blk > 0 && blk_idx / ptrs_per_blk < ptrs_per_blk) {
      uint32_t dindir_blk = inode->inode_block[13];
      if (dindir_blk < ext2_first_data_block || dindir_blk >= ext2_blocks_count) {
         return 0;
      }// if
      static uint32_t dindir_buf1[1024];
      if (ext2_read_block_safe(dindir_blk, dindir_buf1) != 0) {
         return 0;
      }// if
      uint32_t l1_blk = dindir_buf1[blk_idx / ptrs_per_blk];
      if (l1_blk < ext2_first_data_block || l1_blk >= ext2_blocks_count || l1_blk == dindir_blk) {
         return 0;
      }// if
      static uint32_t dindir_buf2[1024];
      if (ext2_read_block_safe(l1_blk, dindir_buf2) != 0) {
         return 0;
      }// if
      uint32_t b = dindir_buf2[blk_idx % ptrs_per_blk];
      return (b >= ext2_first_data_block && b < ext2_blocks_count && b != l1_blk && b != dindir_blk) ? b : 0;
   }// if
   return 0;
}// ext2_get_inode_block

uint32_t ext2_lookup(uint32_t dir_inode_no, const char *name) {
   if (dir_inode_no == 0 || name == NULL || !ext2_ready) {
      return 0;
   }// if
   struct ext2_inode dir_inode;
   if (ext2_read_inode(dir_inode_no, &dir_inode) != 0 || (dir_inode.inode_mode & 0xF000) != 0x4000) {
      return 0;
   }// if
   uint32_t name_len = 0;
   while (name[name_len] != '\0' && name[name_len] != '/' && name_len < 256) {
      name_len++;
   }// while
   if (name_len == 0 || name_len > 255) {
      return (name_len == 0) ? dir_inode_no : 0;
   }// if

   static uint8_t dir_buffer[4096];
   uint32_t total_blocks = (dir_inode.inode_size + ext2_block_size - 1) / ext2_block_size;
   if (dir_inode.inode_size == 0 || total_blocks > 65536) {
      total_blocks = (dir_inode.inode_size == 0) ? 0 : 65536;
   }// if

   for (uint32_t i = 0; i < total_blocks; i++) {
      uint32_t blk = ext2_get_inode_block(&dir_inode, i);
      if (blk == 0) {
         continue;
      }// if
      if (ext2_read_block_safe(blk, dir_buffer) != 0) {
         continue;
      }// if
      uint32_t offset = 0;
      while (offset + 8 <= ext2_block_size) {
         struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(dir_buffer + offset);
         if (entry->rec_len < 8 || offset + entry->rec_len > ext2_block_size) {
            break;
         }// if
         if (8 + (uint32_t)entry->name_len <= (uint32_t)entry->rec_len && dir_entry_matches(entry, name, name_len)) {
            return entry->inode;
         }// if
         offset += entry->rec_len;
      }// while
   }// for
   return 0;
}// ext2_lookup

uint32_t ext2_resolve_path(const char *path) {
   if (path == NULL || *path == '\0' || !ext2_ready) {
      return 0;
   }// if
   uint32_t current_inode = 2;
   uint32_t depth = 0;
   while (*path == '/') {
      path++;
   }// while
   if (*path == '\0') {
      return current_inode;
   }// if

   while (*path != '\0') {
      if (++depth > 128) {
         return 0;
      }// if
      uint32_t next_inode = ext2_lookup(current_inode, path);
      if (next_inode == 0) {
         return 0;
      }// if
      current_inode = next_inode;
      while (*path != '\0' && *path != '/') {
         path++;
      }// while
      while (*path == '/') {
         path++;
      }// while
   }// while
   return current_inode;
}// ext2_resolve_path

int ext2_iterate_dir(const char *path, ext2_dir_cb callback) {
   if (path == NULL || callback == NULL || !ext2_ready) {
      return -1;
   }// if
   uint32_t dir_ino = ext2_resolve_path(path);
   if (dir_ino == 0) {
      return -1;
   }// if
   struct ext2_inode dir_inode;
   if (ext2_read_inode(dir_ino, &dir_inode) != 0) {
      return -1;
   }// if
   if ((dir_inode.inode_mode & 0xF000) != 0x4000) {
      return -1;
   }// if
   static uint8_t dir_buffer[4096];
   uint32_t total_blocks = (dir_inode.inode_size + ext2_block_size - 1) / ext2_block_size;
   if (dir_inode.inode_size == 0 || total_blocks > 65536) {
      total_blocks = (dir_inode.inode_size == 0) ? 0 : 65536;
   }// if
   for (uint32_t i = 0; i < total_blocks; i++) {
      uint32_t blk = ext2_get_inode_block(&dir_inode, i);
      if (blk == 0) {
         continue;
      }// if
      if (ext2_read_block_safe(blk, dir_buffer) != 0) {
         continue;
      }// if
      uint32_t offset = 0;
      while (offset + 8 <= ext2_block_size) {
         struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(dir_buffer + offset);
         if (entry->rec_len < 8 || offset + entry->rec_len > ext2_block_size) {
            break;
         }// if
         if (entry->inode != 0 && 8 + (uint32_t)entry->name_len <= (uint32_t)entry->rec_len) {
            callback(entry->name, entry->name_len, entry->inode, entry->file_type);
         }// if
         offset += entry->rec_len;
      }// while
   }// for
   return 0;
}// ext2_iterate_dir

static void print_dir_entry(const char *name, uint32_t name_len, uint32_t inode, uint8_t file_type) {
   (void)file_type;
   char name_buf[256];
   if (name_len > 255) {
      name_len = 255;
   }// if
   for (uint32_t i = 0; i < name_len; i++) {
      name_buf[i] = name[i];
   }// for
   name_buf[name_len] = '\0';
   print_character(name_buf);
   print_character(" [inode: ");
   print_dec(inode);
   print_character("]\n");
}// print_dir_entry

int ext2_list_dir(const char *path) {
   return ext2_iterate_dir(path, print_dir_entry);
}// ext2_list_dir

int ext2_read(const char *path, void *buffer, uint32_t count, uint32_t offset) {
   if (path == NULL || buffer == NULL || !ext2_ready) {
      return -1;
   }// if
   uint32_t ino = ext2_resolve_path(path);
   if (ino == 0) {
      return -1;
   }// if
   struct ext2_inode inode;
   if (ext2_read_inode(ino, &inode) != 0) {
      return -1;
   }// if
   if (offset >= inode.inode_size) {
      return 0;
   }// if

   if (count > inode.inode_size - offset) {
      count = inode.inode_size - offset;
   }// if

   uint32_t bytes_read = 0;
   static uint8_t blk_buf[4096];
   while (bytes_read < count) {
      uint32_t curr_pos = offset + bytes_read;
      uint32_t blk_idx = curr_pos / ext2_block_size;
      uint32_t blk_off = curr_pos % ext2_block_size;
      uint32_t to_copy = ext2_block_size - blk_off;
      if (to_copy > count - bytes_read) {
         to_copy = count - bytes_read;
      }// if
      uint32_t physical_blk = ext2_get_inode_block(&inode, blk_idx);
      if (physical_blk == 0) {
         memset((uint8_t *)buffer + bytes_read, 0, to_copy);
      } else {
         if (ext2_read_block_safe(physical_blk, blk_buf) != 0) {
            break;
         }// if
         memcpy((uint8_t *)buffer + bytes_read, blk_buf + blk_off, to_copy);
      }// else
      bytes_read += to_copy;
   }// while
   return (int)bytes_read;
}// ext2_read

int ext2_update(const char *path, uint32_t mode, uint32_t mtime) {
   if (path == NULL || !ext2_ready) {
      return -1;
   }// if
   uint32_t ino = ext2_resolve_path(path);
   if (ino == 0) {
      return -1;
   }// if
   struct ext2_inode inode;
   if (ext2_read_inode(ino, &inode) != 0) {
      return -1;
   }// if
   if (mode != 0) {
      if ((mode & 0xF000) != 0) {
         inode.inode_mode = (uint16_t)mode;
      } else {
         inode.inode_mode = (inode.inode_mode & 0xF000) | ((uint16_t)mode & 0x0FFF);
      }// else
   }// if
   if (mtime != 0) {
      inode.inode_mtime = mtime;
   }// if
   return (ext2_write_inode(ino, &inode) == 0) ? 0 : -1;
}// ext2_update

static void ext2_free_block(uint32_t blk) {
   if (!ext2_ready || blk < ext2_first_data_block || blk >= ext2_blocks_count || ext2_blocks_per_group == 0) {
      return;
   }// if
   uint32_t rel_blk = blk - ext2_first_data_block;
   uint32_t group = rel_blk / ext2_blocks_per_group;
   uint32_t bit_index = rel_blk % ext2_blocks_per_group;
   if (group >= ext2_total_groups) {
      return;
   }// if
   if (bit_index / 8 >= ext2_block_size || bit_index / 8 >= 4096) {
      return;
   }// if
   struct ext2_group_desc bgd;
   if (ext2_get_group_desc(group, &bgd) != 0) {
      return;
   }// if
   if (bgd.bg_block_bitmap < ext2_first_data_block || bgd.bg_block_bitmap >= ext2_blocks_count) {
      return;
   }// if
   static uint8_t bmap[4096];
   if (ext2_read_block_safe(bgd.bg_block_bitmap, bmap) != 0) {
      return;
   }// if
   if ((bmap[bit_index / 8] & (1 << (bit_index % 8))) == 0) {
      return;
   }// if
   bmap[bit_index / 8] &= ~(1 << (bit_index % 8));
   if (ext2_write_block_safe(bgd.bg_block_bitmap, bmap) != 0) {
      return;
   }// if
   bgd.bg_free_blocks_count++;
   ext2_put_group_desc(group, &bgd);
}// ext2_free_block

static void ext2_free_inode_record(uint32_t inode_no) {
   if (!ext2_ready || inode_no == 0 || inode_no > ext2_inodes_count || ext2_inodes_per_group == 0) {
      return;
   }// if
   uint32_t group = (inode_no - 1) / ext2_inodes_per_group;
   uint32_t bit_index = (inode_no - 1) % ext2_inodes_per_group;
   if (group >= ext2_total_groups) {
      return;
   }// if
   if (bit_index / 8 >= ext2_block_size || bit_index / 8 >= 4096) {
      return;
   }// if
   struct ext2_group_desc bgd;
   if (ext2_get_group_desc(group, &bgd) != 0) {
      return;
   }// if
   if (bgd.bg_inode_bitmap < ext2_first_data_block || bgd.bg_inode_bitmap >= ext2_blocks_count) {
      return;
   }// if
   static uint8_t imap[4096];
   if (ext2_read_block_safe(bgd.bg_inode_bitmap, imap) != 0) {
      return;
   }// if
   if ((imap[bit_index / 8] & (1 << (bit_index % 8))) == 0) {
      return;
   }// if
   imap[bit_index / 8] &= ~(1 << (bit_index % 8));
   if (ext2_write_block_safe(bgd.bg_inode_bitmap, imap) != 0) {
      return;
   }// if
   bgd.bg_free_inodes_count++;
   ext2_put_group_desc(group, &bgd);
}// ext2_free_inode_record

static uint32_t ext2_alloc_block(void) {
   if (!ext2_ready || ext2_blocks_per_group == 0 || ext2_total_groups == 0) {
      return 0;
   }// if
   for (uint32_t g = 0; g < ext2_total_groups; g++) {
      struct ext2_group_desc bgd;
      if (ext2_get_group_desc(g, &bgd) != 0 || bgd.bg_free_blocks_count == 0) {
         continue;
      }// if
      if (bgd.bg_block_bitmap < ext2_first_data_block || bgd.bg_block_bitmap >= ext2_blocks_count) {
         continue;
      }// if
      static uint8_t bmap[4096];
      if (ext2_read_block_safe(bgd.bg_block_bitmap, bmap) != 0) {
         continue;
      }// if
      for (uint32_t byte = 0; byte < ext2_block_size && byte < 4096; byte++) {
         if (bmap[byte] != 0xFF) {
            for (uint8_t bit = 0; bit < 8; bit++) {
               uint32_t rel_bit = byte * 8 + bit;
               if (rel_bit >= ext2_blocks_per_group) {
                  break;
               }// if
               if ((bmap[byte] & (1 << bit)) == 0) {
                  uint32_t new_blk = g * ext2_blocks_per_group + rel_bit + ext2_first_data_block;
                  if (new_blk >= ext2_blocks_count) {
                     break;
                  }// if
                  bmap[byte] |= (1 << bit);
                  if (ext2_write_block_safe(bgd.bg_block_bitmap, bmap) != 0) {
                     return 0;
                  }// if
                  if (bgd.bg_free_blocks_count > 0) {
                     bgd.bg_free_blocks_count--;
                  }// if
                  ext2_put_group_desc(g, &bgd);

                  static uint8_t zero_buf[4096];
                  memset(zero_buf, 0, ext2_block_size);
                  ext2_write_block_safe(new_blk, zero_buf);
                  return new_blk;
               }// if
            }// for
         }// if
      }// for
   }// for
   return 0;
}// ext2_alloc_block

static uint32_t ext2_alloc_inode(void) {
   if (!ext2_ready || ext2_inodes_per_group == 0 || ext2_total_groups == 0) {
      return 0;
   }// if
   for (uint32_t g = 0; g < ext2_total_groups; g++) {
      struct ext2_group_desc bgd;
      if (ext2_get_group_desc(g, &bgd) != 0 || bgd.bg_free_inodes_count == 0) {
         continue;
      }// if
      if (bgd.bg_inode_bitmap < ext2_first_data_block || bgd.bg_inode_bitmap >= ext2_blocks_count) {
         continue;
      }// if
      static uint8_t imap[4096];
      if (ext2_read_block_safe(bgd.bg_inode_bitmap, imap) != 0) {
         continue;
      }// if
      for (uint32_t byte = 0; byte < ext2_block_size && byte < 4096; byte++) {
         if (imap[byte] != 0xFF) {
            for (uint8_t bit = 0; bit < 8; bit++) {
               uint32_t rel_bit = byte * 8 + bit;
               if (rel_bit >= ext2_inodes_per_group) {
                  break;
               }// if
               uint32_t new_inode = g * ext2_inodes_per_group + rel_bit + 1;
               if (new_inode > ext2_inodes_count) {
                  break;
               }// if
               if (new_inode < ext2_first_ino) {
                  continue;
               }// if
               if ((imap[byte] & (1 << bit)) == 0) {
                  imap[byte] |= (1 << bit);
                  if (ext2_write_block_safe(bgd.bg_inode_bitmap, imap) != 0) {
                     return 0;
                  }// if
                  if (bgd.bg_free_inodes_count > 0) {
                     bgd.bg_free_inodes_count--;
                  }// if
                  ext2_put_group_desc(g, &bgd);
                  return new_inode;
               }// if
            }// for
         }// if
      }// for
   }// for
   return 0;
}// ext2_alloc_inode

static int ext2_add_dir_entry(struct ext2_inode *parent_inode, uint32_t parent_ino, uint32_t new_ino, const char *name, uint32_t name_len, uint8_t file_type) {
   if (parent_inode == NULL || parent_ino == 0 || new_ino == 0 || name == NULL || name_len == 0 || name_len > 255) {
      return -1;
   }// if
   if ((parent_inode->inode_mode & 0xF000) != 0x4000) {
      return -1;
   }// if
   uint32_t needed_len = ((8 + name_len + 3) / 4) * 4;
   uint32_t total_dir_blocks = (parent_inode->inode_size + ext2_block_size - 1) / ext2_block_size;
   if (total_dir_blocks == 0) {
      total_dir_blocks = 1;
   }// if
   if (total_dir_blocks > 12) {
      return -1;
   }// if
   static uint8_t dir_buf[4096];
   for (uint32_t b = 0; b < total_dir_blocks; b++) {
      uint32_t blk = ext2_get_inode_block(parent_inode, b);
      if (blk == 0) {
         continue;
      }// if
      if (ext2_read_block_safe(blk, dir_buf) != 0) {
         continue;
      }// if
      uint32_t offset = 0;
      while (offset + 8 <= ext2_block_size) {
         struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(dir_buf + offset);
         if (entry->rec_len < 8 || offset + entry->rec_len > ext2_block_size) {
            break;
         }// if
         if (entry->inode == 0 && entry->rec_len >= needed_len) {
            entry->inode = new_ino;
            entry->name_len = (uint8_t)name_len;
            entry->file_type = file_type;
            memcpy(entry->name, name, name_len);
            if (ext2_write_block_safe(blk, dir_buf) != 0) {
               return -1;
            }// if
            return (ext2_write_inode(parent_ino, parent_inode) == 0) ? 0 : -1;
         }// if
         if (offset + entry->rec_len == ext2_block_size) {
            uint32_t actual_last_len = ((8 + entry->name_len + 3) / 4) * 4;
            if (entry->rec_len >= actual_last_len + needed_len) {
               uint32_t new_entry_len = entry->rec_len - actual_last_len;
               entry->rec_len = actual_last_len;
               struct ext2_dir_entry *new_entry = (struct ext2_dir_entry *)((uint8_t *)entry + actual_last_len);
               new_entry->inode = new_ino;
               new_entry->rec_len = new_entry_len;
               new_entry->name_len = (uint8_t)name_len;
               new_entry->file_type = file_type;
               memcpy(new_entry->name, name, name_len);
               if (ext2_write_block_safe(blk, dir_buf) != 0) {
                  return -1;
               }// if
               return (ext2_write_inode(parent_ino, parent_inode) == 0) ? 0 : -1;
            }// if
         }// if
         offset += entry->rec_len;
      }// while
   }// for

   if (total_dir_blocks < 12) {
      uint32_t new_blk = ext2_alloc_block();
      if (new_blk == 0) {
         return -1;
      }// if
      parent_inode->inode_block[total_dir_blocks] = new_blk;
      parent_inode->inode_blocks += (ext2_block_size / 512);
      parent_inode->inode_size = (total_dir_blocks + 1) * ext2_block_size;
      memset(dir_buf, 0, ext2_block_size);
      struct ext2_dir_entry *new_entry = (struct ext2_dir_entry *)dir_buf;
      new_entry->inode = new_ino;
      new_entry->rec_len = ext2_block_size;
      new_entry->name_len = (uint8_t)name_len;
      new_entry->file_type = file_type;
      memcpy(new_entry->name, name, name_len);
      if (ext2_write_block_safe(new_blk, dir_buf) != 0) {
         ext2_free_block(new_blk);
         return -1;
      }// if
      return (ext2_write_inode(parent_ino, parent_inode) == 0) ? 0 : -1;
   }// if
   return -1;
}// ext2_add_dir_entry

static int ext2_split_path(const char *path, char *parent_path, size_t parent_path_size, const char **filename, uint32_t *name_len) {
   if (path == NULL || *path == '\0' || parent_path == NULL || filename == NULL || name_len == NULL || parent_path_size < 2) {
      return -1;
   }// if
   uint32_t path_len = 0;
   while (path[path_len] != '\0' && path_len < 1024) {
      path_len++;
   }// while
   if (path_len == 0 || path_len >= 1024) {
      return -1;
   }// if
   while (path_len > 1 && path[path_len - 1] == '/') {
      path_len--;
   }// while
   if (path_len == 1 && path[0] == '/') {
      return -1;
   }// if
   uint32_t last_slash = (uint32_t)-1;
   for (uint32_t i = 0; i < path_len; i++) {
      if (path[i] == '/') {
         last_slash = i;
      }// if
   }// for
   if (last_slash == (uint32_t)-1) {
      parent_path[0] = '/';
      parent_path[1] = '\0';
      *filename = path;
      *name_len = path_len;
   } else if (last_slash == 0) {
      parent_path[0] = '/';
      parent_path[1] = '\0';
      *filename = path + 1;
      *name_len = path_len - 1;
   } else {
      if (last_slash >= parent_path_size) {
         return -1;
      }// if
      for (uint32_t i = 0; i < last_slash; i++) {
         parent_path[i] = path[i];
      }// for
      parent_path[last_slash] = '\0';
      *filename = path + last_slash + 1;
      *name_len = path_len - last_slash - 1;
   }// else
   if (*name_len == 0 || *name_len > 255) {
      return -1;
   }// if
   for (uint32_t i = 0; i < *name_len; i++) {
      if ((*filename)[i] == '/') {
         return -1;
      }// if
   }// for
   if (*name_len == 1 && (*filename)[0] == '.') {
      return -1;
   }// if
   if (*name_len == 2 && (*filename)[0] == '.' && (*filename)[1] == '.') {
      return -1;
   }// if
   return 0;
}// ext2_split_path

uint32_t ext2_create_file(const char *path) {
   if (!ext2_ready) {
      return -1;
   }// if
   char parent_path[256];
   const char *filename = NULL;
   uint32_t name_len = 0;
   if (ext2_split_path(path, parent_path, sizeof(parent_path), &filename, &name_len) != 0) {
      return -1;
   }// if
   uint32_t parent_ino = ext2_resolve_path(parent_path);
   if (parent_ino == 0) {
      return -1;
   }// if
   struct ext2_inode parent_inode;
   if (ext2_read_inode(parent_ino, &parent_inode) != 0) {
      return -1;
   }// if
   if ((parent_inode.inode_mode & 0xF000) != 0x4000) {
      return -1;
   }// if
   if (ext2_lookup(parent_ino, filename) != 0) {
      return 0;
   }// if

   uint32_t new_ino = ext2_alloc_inode();
   if (new_ino == 0) {
      return -1;
   }// if
   struct ext2_inode new_inode;
   memset(&new_inode, 0, sizeof(struct ext2_inode));
   new_inode.inode_mode = 0x81A4;
   new_inode.inode_size = 0;
   new_inode.inode_links_count = 1;
   new_inode.inode_blocks = 0;
   new_inode.inode_block[0] = 0;
   if (ext2_write_inode(new_ino, &new_inode) != 0) {
      ext2_free_inode_record(new_ino);
      return -1;
   }// if

   if (ext2_add_dir_entry(&parent_inode, parent_ino, new_ino, filename, name_len, 1) != 0) {
      ext2_free_inode_record(new_ino);
      return -1;
   }// if
   return 0;
}// ext2_create_file

uint32_t ext2_mkdir(const char *path) {
   if (!ext2_ready) {
      return -1;
   }// if
   char parent_path[256];
   const char *filename = NULL;
   uint32_t name_len = 0;
   if (ext2_split_path(path, parent_path, sizeof(parent_path), &filename, &name_len) != 0) {
      return -1;
   }// if
   uint32_t parent_ino = ext2_resolve_path(parent_path);
   if (parent_ino == 0) {
      return -1;
   }// if
   struct ext2_inode parent_inode;
   if (ext2_read_inode(parent_ino, &parent_inode) != 0) {
      return -1;
   }// if
   if ((parent_inode.inode_mode & 0xF000) != 0x4000) {
      return -1;
   }// if
   if (ext2_lookup(parent_ino, filename) != 0) {
      return 0;
   }// if

   uint32_t new_ino = ext2_alloc_inode();
   uint32_t new_blk = ext2_alloc_block();
   if (new_ino == 0 || new_blk == 0) {
      if (new_ino != 0) {
         ext2_free_inode_record(new_ino);
      }// if
      if (new_blk != 0) {
         ext2_free_block(new_blk);
      }// if
      return -1;
   }// if

   struct ext2_inode new_inode;
   memset(&new_inode, 0, sizeof(struct ext2_inode));
   new_inode.inode_mode = 0x41ED;
   new_inode.inode_size = ext2_block_size;
   new_inode.inode_links_count = 2;
   new_inode.inode_blocks = ext2_block_size / 512;
   new_inode.inode_block[0] = new_blk;
   if (ext2_write_inode(new_ino, &new_inode) != 0) {
      ext2_free_block(new_blk);
      ext2_free_inode_record(new_ino);
      return -1;
   }// if

   static uint8_t dir_buf[4096];
   memset(dir_buf, 0, ext2_block_size);
   struct ext2_dir_entry *dot = (struct ext2_dir_entry *)dir_buf;
   dot->inode = new_ino;
   dot->rec_len = 12;
   dot->name_len = 1;
   dot->file_type = 2;
   dot->name[0] = '.';

   struct ext2_dir_entry *dotdot = (struct ext2_dir_entry *)(dir_buf + 12);
   dotdot->inode = parent_ino;
   dotdot->rec_len = ext2_block_size - 12;
   dotdot->name_len = 2;
   dotdot->file_type = 2;
   dotdot->name[0] = '.';
   dotdot->name[1] = '.';
   if (ext2_write_block_safe(new_blk, dir_buf) != 0) {
      ext2_free_block(new_blk);
      ext2_free_inode_record(new_ino);
      return -1;
   }// if

   if (ext2_add_dir_entry(&parent_inode, parent_ino, new_ino, filename, name_len, 2) != 0) {
      ext2_free_block(new_blk);
      ext2_free_inode_record(new_ino);
      return -1;
   }// if

   parent_inode.inode_links_count++;
   ext2_write_inode(parent_ino, &parent_inode);
   return 0;
}// ext2_mkdir

static bool ext2_is_dir_empty(const struct ext2_inode *dir) {
   if (dir == NULL || (dir->inode_mode & 0xF000) != 0x4000) {
      return false;
   }// if
   uint32_t total_blocks = (dir->inode_size + ext2_block_size - 1) / ext2_block_size;
   if (total_blocks > 12) {
      total_blocks = 12;
   }// if
   static uint8_t chk_buf[4096];
   for (uint32_t b = 0; b < total_blocks; b++) {
      uint32_t blk = ext2_get_inode_block(dir, b);
      if (blk == 0) {
         continue;
      }// if
      if (ext2_read_block_safe(blk, chk_buf) != 0) {
         return false;
      }// if
      uint32_t offset = 0;
      while (offset + 8 <= ext2_block_size) {
         struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(chk_buf + offset);
         if (entry->rec_len < 8 || offset + entry->rec_len > ext2_block_size) {
            break;
         }// if
         if (entry->inode != 0) {
            bool is_dot = (entry->name_len == 1 && entry->name[0] == '.');
            bool is_dotdot = (entry->name_len == 2 && entry->name[0] == '.' && entry->name[1] == '.');
            if (!is_dot && !is_dotdot) {
               return false;
            }// if
         }// if
         offset += entry->rec_len;
      }// while
   }// for
   return true;
}// ext2_is_dir_empty

int ext2_remove(const char *path) {
   if (!ext2_ready) {
      return -1;
   }// if
   char parent_path[256];
   const char *filename = NULL;
   uint32_t name_len = 0;
   if (ext2_split_path(path, parent_path, sizeof(parent_path), &filename, &name_len) != 0) {
      return -1;
   }// if
   uint32_t dir_ino = ext2_resolve_path(parent_path);
   if (dir_ino == 0) {
      return -1;
   }// if
   struct ext2_inode dir_inode;
   if (ext2_read_inode(dir_ino, &dir_inode) != 0) {
      return -1;
   }// if
   if ((dir_inode.inode_mode & 0xF000) != 0x4000) {
      return -1;
   }// if

   static uint8_t dir_buffer[4096];
   uint32_t target_ino = 0;
   uint32_t match_blk = 0;
   uint32_t match_offset = 0;
   uint32_t prev_offset = 0;
   bool has_prev = false;
   bool entry_found = false;
   uint32_t total_dir_blocks = (dir_inode.inode_size + ext2_block_size - 1) / ext2_block_size;
   if (dir_inode.inode_size == 0 || total_dir_blocks > 65536) {
      total_dir_blocks = (dir_inode.inode_size == 0) ? 0 : 65536;
   }// if

   for (uint32_t i = 0; i < total_dir_blocks && !entry_found; i++) {
      uint32_t blk = ext2_get_inode_block(&dir_inode, i);
      if (blk == 0) {
         continue;
      }// if
      if (ext2_read_block_safe(blk, dir_buffer) != 0) {
         continue;
      }// if
      uint32_t offset = 0;
      has_prev = false;
      while (offset + 8 <= ext2_block_size) {
         struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(dir_buffer + offset);
         if (entry->rec_len < 8 || offset + entry->rec_len > ext2_block_size) {
            break;
         }// if
         if (8 + (uint32_t)entry->name_len <= (uint32_t)entry->rec_len && dir_entry_matches(entry, filename, name_len)) {
            target_ino = entry->inode;
            match_blk = blk;
            match_offset = offset;
            entry_found = true;
            break;
         }// if
         has_prev = true;
         prev_offset = offset;
         offset += entry->rec_len;
      }// while
   }// for

   if (!entry_found || target_ino == 0 || target_ino < ext2_first_ino || target_ino > ext2_inodes_count) {
      return -1;
   }// if
   struct ext2_inode target_inode;
   if (ext2_read_inode(target_ino, &target_inode) != 0) {
      return -1;
   }// if
   if ((target_inode.inode_mode & 0xF000) == 0x4000) {
      if (!ext2_is_dir_empty(&target_inode)) {
         return -1;
      }// if
   }// if

   if (ext2_read_block_safe(match_blk, dir_buffer) != 0) {
      return -1;
   }// if
   struct ext2_dir_entry *cur = (struct ext2_dir_entry *)(dir_buffer + match_offset);
   if (has_prev) {
      struct ext2_dir_entry *prev = (struct ext2_dir_entry *)(dir_buffer + prev_offset);
      prev->rec_len += cur->rec_len;
   } else {
      cur->inode = 0;
   }// else
   if (ext2_write_block_safe(match_blk, dir_buffer) != 0) {
      return -1;
   }// if

   if ((target_inode.inode_mode & 0xF000) == 0x4000) {
      if (dir_inode.inode_links_count > 2) {
         dir_inode.inode_links_count--;
         ext2_write_inode(dir_ino, &dir_inode);
      }// if
   }// if

   if (target_inode.inode_links_count > 0) {
      target_inode.inode_links_count--;
   }// if
   if (target_inode.inode_links_count == 0) {
      for (uint32_t i = 0; i < 12; i++) {
         if (target_inode.inode_block[i] != 0) {
            ext2_free_block(target_inode.inode_block[i]);
            target_inode.inode_block[i] = 0;
         }// if
      }// for
      uint32_t ptrs_per_blk = ext2_block_size / 4;
      if (target_inode.inode_block[12] != 0 && ptrs_per_blk > 0) {
         static uint32_t indir_buf[1024];
         if (ext2_read_block_safe(target_inode.inode_block[12], indir_buf) == 0) {
            for (uint32_t i = 0; i < ptrs_per_blk; i++) {
               if (indir_buf[i] != 0 && indir_buf[i] != target_inode.inode_block[12]) {
                  ext2_free_block(indir_buf[i]);
               }// if
            }// for
         }// if
         ext2_free_block(target_inode.inode_block[12]);
         target_inode.inode_block[12] = 0;
      }// if
      if (target_inode.inode_block[13] != 0 && ptrs_per_blk > 0) {
         static uint32_t dindir_buf1[1024];
         if (ext2_read_block_safe(target_inode.inode_block[13], dindir_buf1) == 0) {
            for (uint32_t i = 0; i < ptrs_per_blk; i++) {
               uint32_t l1 = dindir_buf1[i];
               if (l1 != 0 && l1 != target_inode.inode_block[13]) {
                  static uint32_t dindir_buf2[1024];
                  if (ext2_read_block_safe(l1, dindir_buf2) == 0) {
                     for (uint32_t j = 0; j < ptrs_per_blk; j++) {
                        uint32_t l2 = dindir_buf2[j];
                        if (l2 != 0 && l2 != l1 && l2 != target_inode.inode_block[13]) {
                           ext2_free_block(l2);
                        }// if
                     }// for
                  }// if
                  ext2_free_block(l1);
               }// if
            }// for
         }// if
         ext2_free_block(target_inode.inode_block[13]);
         target_inode.inode_block[13] = 0;
      }// if
      ext2_free_inode_record(target_ino);
      target_inode.inode_dtime = 1;
   }// if
   ext2_write_inode(target_ino, &target_inode);
   return 0;
}// ext2_remove

int ext2_write(const char *path, const void *buffer, uint32_t count, uint32_t offset) {
   if (path == NULL || buffer == NULL || !ext2_ready) {
      return -1;
   }// if
   if (count > 0 && offset > 0xFFFFFFFF - count) {
      return -1;
   }// if
   uint32_t ino = ext2_resolve_path(path);
   if (ino == 0) {
      if (ext2_create_file(path) != 0) {
         return -1;
      }// if
      ino = ext2_resolve_path(path);
   }// if
   if (ino == 0) {
      return -1;
   }// if
   struct ext2_inode inode;
   if (ext2_read_inode(ino, &inode) != 0) {
      return -1;
   }// if
   if ((inode.inode_mode & 0xF000) == 0x4000) {
      return -1;
   }// if

   uint32_t ptrs_per_blk = ext2_block_size / 4;
   if (ptrs_per_blk == 0) {
      return -1;
   }// if
   uint32_t bytes_written = 0;
   static uint8_t blk_buf[4096];
   while (bytes_written < count) {
      uint32_t curr_pos = offset + bytes_written;
      uint32_t blk_idx = curr_pos / ext2_block_size;
      uint32_t blk_off = curr_pos % ext2_block_size;
      uint32_t to_copy = ext2_block_size - blk_off;
      if (to_copy > count - bytes_written) {
         to_copy = count - bytes_written;
      }// if

      uint32_t physical_blk = 0;
      if (blk_idx < 12) {
         physical_blk = inode.inode_block[blk_idx];
         if (physical_blk == 0) {
            physical_blk = ext2_alloc_block();
            if (physical_blk == 0) {
               break;
            }// if
            inode.inode_block[blk_idx] = physical_blk;
            inode.inode_blocks += (ext2_block_size / 512);
         }// if
      } else if (blk_idx - 12 < ptrs_per_blk) {
         uint32_t indir_idx = blk_idx - 12;
         if (inode.inode_block[12] == 0) {
            uint32_t new_indir = ext2_alloc_block();
            if (new_indir == 0) {
               break;
            }// if
            inode.inode_block[12] = new_indir;
            inode.inode_blocks += (ext2_block_size / 512);
         }// if

         static uint32_t indir_table[1024];
         if (ext2_read_block_safe(inode.inode_block[12], indir_table) != 0) {
            break;
         }// if
         physical_blk = indir_table[indir_idx];
         if (physical_blk == 0) {
            physical_blk = ext2_alloc_block();
            if (physical_blk == 0) {
               break;
            }// if
            indir_table[indir_idx] = physical_blk;
            if (ext2_write_block_safe(inode.inode_block[12], indir_table) != 0) {
               ext2_free_block(physical_blk);
               break;
            }// if
            inode.inode_blocks += (ext2_block_size / 512);
         }// if
      } else {
         uint32_t d_idx = (blk_idx - 12) - ptrs_per_blk;
         if (ptrs_per_blk > 0 && d_idx / ptrs_per_blk >= ptrs_per_blk) {
            break;
         }// if

         if (inode.inode_block[13] == 0) {
            uint32_t new_dindir = ext2_alloc_block();
            if (new_dindir == 0) {
               break;
            }// if
            inode.inode_block[13] = new_dindir;
            inode.inode_blocks += (ext2_block_size / 512);
         }// if

         static uint32_t dindir_table1[1024];
         if (ext2_read_block_safe(inode.inode_block[13], dindir_table1) != 0) {
            break;
         }// if
         uint32_t l1_idx = d_idx / ptrs_per_blk;
         uint32_t l2_idx = d_idx % ptrs_per_blk;
         uint32_t l1_blk = dindir_table1[l1_idx];
         if (l1_blk == 0) {
            l1_blk = ext2_alloc_block();
            if (l1_blk == 0) {
               break;
            }// if
            dindir_table1[l1_idx] = l1_blk;
            if (ext2_write_block_safe(inode.inode_block[13], dindir_table1) != 0) {
               ext2_free_block(l1_blk);
               break;
            }// if
            inode.inode_blocks += (ext2_block_size / 512);
         }// if

         static uint32_t dindir_table2[1024];
         if (ext2_read_block_safe(l1_blk, dindir_table2) != 0) {
            break;
         }// if
         physical_blk = dindir_table2[l2_idx];
         if (physical_blk == 0) {
            physical_blk = ext2_alloc_block();
            if (physical_blk == 0) {
               break;
            }// if
            dindir_table2[l2_idx] = physical_blk;
            if (ext2_write_block_safe(l1_blk, dindir_table2) != 0) {
               ext2_free_block(physical_blk);
               break;
            }// if
            inode.inode_blocks += (ext2_block_size / 512);
         }// if
      }// else

      if (physical_blk == 0) {
         break;
      }// if
      if (to_copy < ext2_block_size) {
         ext2_read_block_safe(physical_blk, blk_buf);
      }// if
      memcpy(blk_buf + blk_off, (const uint8_t *)buffer + bytes_written, to_copy);
      if (ext2_write_block_safe(physical_blk, blk_buf) != 0) {
         break;
      }// if
      bytes_written += to_copy;
   }// while

   if (offset + bytes_written > inode.inode_size) {
      inode.inode_size = offset + bytes_written;
   }// if
   if (ext2_write_inode(ino, &inode) != 0) {
      return -1;
   }// if
   return (int)bytes_written;
}// ext2_write

void ext2_driver_process(void) {
   init_ext2();
   struct fs_ipc_req req;
   uint32_t status;
   while (1) {
      uint32_t sender = user_ipc_recv(0, &req);
      if (sender == (uint32_t)-1) {
         user_yield();
         continue;
      }// if
      req.path[sizeof(req.path) - 1] = '\0';
      if (!ext2_ready) {
         status = (uint32_t)-1;
      } else if (req.cmd == 2) {
         status = ext2_mkdir(req.path);
      } else if (req.cmd == 3) {
         status = ext2_create_file(req.path);
      } else if (req.cmd == 4) {
         status = (uint32_t)ext2_remove(req.path);
      } else {
         status = (uint32_t)-1;
      }// else
      user_ipc_send(sender, (void *)(uintptr_t)status);
   }// while
}// ext2_driver_process

int ext2_virtio_transfer(uint32_t sector, void *buffer, bool is_write) {
   if (buffer == NULL) {
      return -1;
   }// if
   if (ext2_ramdisk_base != 0) {
      uint64_t offset = (uint64_t)sector * 512;
      if (offset + 512 > ext2_ramdisk_size) {
         return -1;
      }// if
      if (!is_write) {
         memcpy(buffer, (const void *)(ext2_ramdisk_base + offset), 512);
      } else {
         memcpy((void *)(ext2_ramdisk_base + offset), buffer, 512);
      }// else
      return 0;
   }// if
   uint32_t virtio_pid = user_get_pid_by_gid(VIRTIO_GID);
   if (virtio_pid == (uint32_t)-1) {
      return -1;
   }// if
   struct virtio_ipc_req req;
   req.cmd = is_write ? 1 : 0;
   req.sector = sector;
   req.buffer = buffer;
   uint32_t status = (uint32_t)-1;
   if (user_ipc_call(virtio_pid, &req, &status) != 0) {
      return -1;
   }// if
   return (int)status;
}// ext2_virtio_transfer

void create_ext2_process(void) {
   infon("Starting ext2 process");
   uint32_t pid = create_process((void *)ext2_driver_process, FS_GID);
   struct process *ext2_process = get_process_by_pid(pid);
   if (ext2_process) {
      uint32_t gid = ext2_process->gid;
      uint32_t sid = generate_sid(gid, (uintptr_t)ext2_driver_process);
      ext2_process->sid = sid;
   }// if
   successn("ext2 process started");
}// create_ext2_process
