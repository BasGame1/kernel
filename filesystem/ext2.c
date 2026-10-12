#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "../include/hardware.h"
#include "../include/kernel.h"
#include "../include/userspace.h"
#include "../include/gid.h"

int ext2_virtio_transfer(uint32_t sector, void *buffer, bool is_write);

static uint32_t ext2_block_size = 1024;
static uint32_t ext2_inode_table_block = 0;
static uint32_t ext2_inode_size = 128;

void ext2_read_block(uint32_t block, void *buffer) {
   uint32_t sectors_per_block = ext2_block_size / 512;
   uint32_t start_sector = block * sectors_per_block;
   for (uint32_t i = 0; i < sectors_per_block; i++) {
      if (ext2_virtio_transfer(start_sector + i, (uint8_t *)buffer + (i * 512), false) != 0) {
         error("Read failed at sector ");
         print_dec(start_sector + i);
         print_character("\n");
         return;
      }// if check fail
   }// for loop to all the sectors
}// ext2_read_block

void ext2_write_block(uint32_t block, void *buffer) {
   uint32_t sectors_per_block = ext2_block_size / 512;
   uint32_t start_sector = block * sectors_per_block;
   for (uint32_t i = 0; i < sectors_per_block; i++) {
      if (ext2_virtio_transfer(start_sector + i, (uint8_t *)buffer + (i * 512), true) != 0) {
         error("Write failed at sector ");
         print_dec(start_sector + i);
         print_character("\n");
         return;
      }// if check fail
   }// for loop to all the sectors
}// ext2_write_block

void init_ext2() {
   static uint8_t buffer[1024];
   if (ext2_virtio_transfer(2, buffer, false) != 0 || ext2_virtio_transfer(3, buffer + 512, false) != 0) {
      errorn("Failed to read ext2 superblock");
      return;
   }// if to check the read of ext2
   struct ext2_super_block *sb = (struct ext2_super_block *)buffer;
   if (sb->ext2_magic == EXT2_MAGIC) {
      success("ext2 magic found: ");
      print_hex(sb->ext2_magic);
      print_character("\n");
   } else {
      error("ext2 magic not found:  ");
      print_hex(sb->ext2_magic);
      print_character("\n");
      return;
   }// check for magic error
   ext2_block_size = 1024 << sb->ext2_log_block_size;
   info("Ext2 block size: ");
   print_dec(ext2_block_size);
   print_character("\n");
   /* bgd block */
   uint32_t bgd_block = (ext2_block_size == 1024) ? 2 : 1;
   static uint8_t bgd_buffer[4096];
   ext2_read_block(bgd_block, bgd_buffer);
   struct ext2_group_desc *bgd = (struct ext2_group_desc *)bgd_buffer;
   info("Bgd inode table is: ");
   ext2_inode_table_block = bgd[0].bg_inode_table;
   print_dec(ext2_inode_table_block);
   print_character("\n");
   /* inodes */
   ext2_inode_size = (sb->ext2_rev_level >= 1 && sb->ext2_inode_size != 0) ? sb->ext2_inode_size : 128;
   uint32_t inode_id = 2;
   uint32_t inode_index = inode_id - 1;
   uint32_t inode_block = ext2_inode_table_block + (inode_index * ext2_inode_size) / ext2_block_size;
   uint32_t byte_offset = (inode_index * ext2_inode_size) % ext2_block_size;
   ext2_read_block(inode_block, buffer);
   struct ext2_inode *root_inode = (struct ext2_inode *)(buffer + byte_offset);
   uint32_t dir_size = root_inode->inode_size;
   uint32_t block_number = root_inode->inode_block[0];
   info("Dir size: ");
   print_dec(dir_size);
   print_character("\n");
   info("Dir block 0: ");
   print_dec(block_number);
   print_character("\n");
   static uint8_t dir_buffer[4096];
   ext2_read_block(root_inode->inode_block[0], dir_buffer);
   uint32_t offset = 0;
   while (offset < root_inode->inode_size && offset < ext2_block_size) {
      struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(dir_buffer + offset);
      if (entry->rec_len == 0) {
         infon("Exiting loop");
         break;
      }
      if (entry->inode != 0) {
         char name_buf[256];
         for (uint8_t i = 0; i < entry->name_len; i++) {
            name_buf[i] = entry->name[i];
         }// for loop
         name_buf[entry->name_len] = '\0';
         print_character(name_buf);
         print_character("\n");
      }// if to check if value is not 0
      offset += entry->rec_len;
   }// while
   #ifdef __DEBUG__
     debugn("Testing ext2_list_dir(\"/\"):\n");
     ext2_list_dir("/");
     #ifdef __QEMU__
        static char test_file_buf[128];
        int r = ext2_read("/hello.txt", test_file_buf, 127, 0);
        if (r > 0) {
           test_file_buf[r] = '\0';
           success("Read /hello.txt: ");
           print_character(test_file_buf);
           print_character("\n");
        } else {
           errorn("Failed to read /hello.txt");
        }
     #endif
   #endif
}// init_ext2

uint32_t ext2_read_inode(uint32_t inode_no, struct ext2_inode *out) {
   if (inode_no == 0 || out == NULL) {
      return 1;
   }// if
   uint32_t index = inode_no - 1;
   uint32_t inode_block = ext2_inode_table_block + (index * ext2_inode_size) / ext2_block_size;
   uint32_t byte_offset = (index * ext2_inode_size) % ext2_block_size;
   static uint8_t buffer[4096];
   ext2_read_block(inode_block, buffer);
   struct ext2_inode *src = (struct ext2_inode *)(buffer + byte_offset);
   *out = *src;
   return 0;
}// ext2_read_inode

uint32_t ext2_write_inode(uint32_t inode_no, const struct ext2_inode *in) {
   if (inode_no == 0 || in == NULL) {
      return 1;
   }// if
   uint32_t index = inode_no - 1;
   uint32_t inode_block = ext2_inode_table_block + (index * ext2_inode_size) / ext2_block_size;
   uint32_t byte_offset = (index * ext2_inode_size) % ext2_block_size;
   static uint8_t buffer[4096];
   ext2_read_block(inode_block, buffer);
   struct ext2_inode *dst = (struct ext2_inode *)(buffer + byte_offset);
   *dst = *in;
   ext2_write_block(inode_block, buffer);
   return 0;
}// ext2_write_inode

uint32_t ext2_lookup(uint32_t dir_inode_no, const char *name) {
   if (dir_inode_no == 0 || name == NULL) {
      return 0;
   }// if
   struct ext2_inode dir_inode;
   if (ext2_read_inode(dir_inode_no, &dir_inode) != 0) {
      return 0;
   }// if
   if ((dir_inode.inode_mode & 0xF000) != 0x4000) {
      return 0;
   }// if
   uint32_t name_len = 0;
   while (name[name_len] != '\0' && name[name_len] != '/') {
      name_len++;
   }// while to tokenize
   if (name_len == 0) {
      return dir_inode_no;
   }// if
   static uint8_t dir_buffer[4096];
   for (uint32_t i = 0; i < 12; i++) {
      uint32_t blk = dir_inode.inode_block[i];
      if (blk == 0) {
         continue;
      }//if to follow next block
      ext2_read_block(blk, dir_buffer);
      uint32_t offset = 0;
      while (offset < ext2_block_size) {
         struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(dir_buffer + offset);
         if (entry->rec_len == 0) {
            break;
         }// security check
         if (dir_entry_matches(entry, name, name_len)) {
            return entry->inode;
         }// check if it is the one
         offset += entry->rec_len;
      }// while
   }// for
   return 0; // should be 404, bc not found
}// ext2_lookup

uint32_t ext2_resolve_path(const char *path) {
   if (path == NULL || *path == '\0') {
      return 0;
   }// if
   uint32_t current_inode = 2;
   while (*path == '/') {
      path++;
   }// while
   if (*path == '\0') {
      return current_inode;
   }// if
   while (*path != '\0') {
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
   if (path == NULL || callback == NULL) {
      return -1;
   }
   uint32_t dir_ino = ext2_resolve_path(path);
   if (dir_ino == 0) {
      return -1;
   }
   struct ext2_inode dir_inode;
   if (ext2_read_inode(dir_ino, &dir_inode) != 0) {
      return -1;
   }
   if ((dir_inode.inode_mode & 0xF000) != 0x4000) {
      return -1;
   }
   static uint8_t dir_buffer[4096];
   for (uint32_t i = 0; i < 12; i++) {
      uint32_t blk = dir_inode.inode_block[i];
      if (blk == 0) {
         continue;
      }
      ext2_read_block(blk, dir_buffer);
      uint32_t offset = 0;
      while (offset < ext2_block_size) {
         struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(dir_buffer + offset);
         if (entry->rec_len == 0) {
            break;
         }
         if (entry->inode != 0) {
            callback(entry->name, entry->name_len, entry->inode, entry->file_type);
         }
         offset += entry->rec_len;
      }
   }
   return 0;
}

static void print_dir_entry(const char *name, uint32_t name_len, uint32_t inode, uint8_t file_type) {
   (void)file_type;
   char name_buf[256];
   if (name_len > 255) {
      name_len = 255;
   }
   for (uint32_t i = 0; i < name_len; i++) {
      name_buf[i] = name[i];
   }
   name_buf[name_len] = '\0';
   print_character(name_buf);
   print_character(" [inode: ");
   print_dec(inode);
   print_character("]\n");
}

int ext2_list_dir(const char *path) {
   return ext2_iterate_dir(path, print_dir_entry);
}

static uint32_t ext2_get_inode_block(const struct ext2_inode *inode, uint32_t blk_idx) {
   if (blk_idx < 12) {
      return inode->inode_block[blk_idx];
   }
   blk_idx -= 12;
   uint32_t ptrs_per_blk = ext2_block_size / 4;
   if (blk_idx < ptrs_per_blk) {
      uint32_t indir_blk = inode->inode_block[12];
      if (indir_blk == 0) {
         return 0;
      }
      static uint32_t indir_buf[1024];
      ext2_read_block(indir_blk, indir_buf);
      return indir_buf[blk_idx];
   }
   return 0;
}

int ext2_read(const char *path, void *buffer, uint32_t count, uint32_t offset) {
   if (path == NULL || buffer == NULL) {
      return -1;
   }
   uint32_t ino = ext2_resolve_path(path);
   if (ino == 0) {
      return -1;
   }
   struct ext2_inode inode;
   if (ext2_read_inode(ino, &inode) != 0) {
      return -1;
   }
   if (offset >= inode.inode_size) {
      return 0;
   }
   if (offset + count > inode.inode_size) {
      count = inode.inode_size - offset;
   }
   uint32_t bytes_read = 0;
   static uint8_t blk_buf[4096];
   while (bytes_read < count) {
      uint32_t curr_pos = offset + bytes_read;
      uint32_t blk_idx = curr_pos / ext2_block_size;
      uint32_t blk_off = curr_pos % ext2_block_size;
      uint32_t to_copy = ext2_block_size - blk_off;
      if (to_copy > count - bytes_read) {
         to_copy = count - bytes_read;
      }
      uint32_t physical_blk = ext2_get_inode_block(&inode, blk_idx);
      if (physical_blk == 0) {
         memset((uint8_t *)buffer + bytes_read, 0, to_copy);
      } else {
         ext2_read_block(physical_blk, blk_buf);
         memcpy((uint8_t *)buffer + bytes_read, blk_buf + blk_off, to_copy);
      }
      bytes_read += to_copy;
   }
   return (int)bytes_read;
}

int ext2_update(const char *path, uint32_t mode, uint32_t mtime) {
   if (path == NULL) {
      return -1;
   }
   uint32_t ino = ext2_resolve_path(path);
   if (ino == 0) {
      return -1;
   }
   struct ext2_inode inode;
   if (ext2_read_inode(ino, &inode) != 0) {
      return -1;
   }
   if (mode != 0) {
      inode.inode_mode = (uint16_t)mode;
   }
   if (mtime != 0) {
      inode.inode_mtime = mtime;
   }
   return (ext2_write_inode(ino, &inode) == 0) ? 0 : -1;
}

static uint32_t ext2_alloc_block(void) {
   uint32_t bgd_block = (ext2_block_size == 1024) ? 2 : 1;
   static uint8_t bgd_buf[4096];
   ext2_read_block(bgd_block, bgd_buf);
   struct ext2_group_desc *bgd = (struct ext2_group_desc *)bgd_buf;
   if (bgd[0].bg_free_blocks_count == 0) {
      return 0;
   }
   static uint8_t bmap[4096];
   ext2_read_block(bgd[0].bg_block_bitmap, bmap);
   for (uint32_t byte = 0; byte < ext2_block_size; byte++) {
      if (bmap[byte] != 0xFF) {
         for (uint8_t bit = 0; bit < 8; bit++) {
            if ((bmap[byte] & (1 << bit)) == 0) {
               bmap[byte] |= (1 << bit);
               ext2_write_block(bgd[0].bg_block_bitmap, bmap);
               bgd[0].bg_free_blocks_count--;
               ext2_write_block(bgd_block, bgd_buf);
               uint32_t new_blk = byte * 8 + bit + 1;
               static uint8_t zero_buf[4096];
               memset(zero_buf, 0, ext2_block_size);
               ext2_write_block(new_blk, zero_buf);
               return new_blk;
            }
         }
      }
   }
   return 0;
}

int ext2_write(const char *path, const void *buffer, uint32_t count, uint32_t offset) {
   if (path == NULL || buffer == NULL) {
      return -1;
   }
   uint32_t ino = ext2_resolve_path(path);
   if (ino == 0) {
      ext2_create_file(path);
      ino = ext2_resolve_path(path);
   }
   struct ext2_inode inode;
   if (ext2_read_inode(ino, &inode) != 0) {
      return -1;
   }
   uint32_t bytes_written = 0;
   static uint8_t blk_buf[4096];
   while (bytes_written < count) {
      uint32_t curr_pos = offset + bytes_written;
      uint32_t blk_idx = curr_pos / ext2_block_size;
      uint32_t blk_off = curr_pos % ext2_block_size;
      uint32_t to_copy = ext2_block_size - blk_off;
      if (to_copy > count - bytes_written) {
         to_copy = count - bytes_written;
      }
      if (blk_idx >= 12) {
         break;
      }
      uint32_t physical_blk = inode.inode_block[blk_idx];
      if (physical_blk == 0) {
         physical_blk = ext2_alloc_block();
         if (physical_blk == 0) {
            break;
         }
         inode.inode_block[blk_idx] = physical_blk;
         inode.inode_blocks += (ext2_block_size / 512);
      }
      ext2_read_block(physical_blk, blk_buf);
      memcpy(blk_buf + blk_off, (const uint8_t *)buffer + bytes_written, to_copy);
      ext2_write_block(physical_blk, blk_buf);
      bytes_written += to_copy;
   }
   if (offset + bytes_written > inode.inode_size) {
      inode.inode_size = offset + bytes_written;
   }
   ext2_write_inode(ino, &inode);
   return (int)bytes_written;
}

static void ext2_free_block(uint32_t blk) {
   if (blk == 0) {
      return;
   }
   uint32_t bgd_block = (ext2_block_size == 1024) ? 2 : 1;
   static uint8_t bgd_buf[4096];
   ext2_read_block(bgd_block, bgd_buf);
   struct ext2_group_desc *bgd = (struct ext2_group_desc *)bgd_buf;
   static uint8_t bmap[4096];
   ext2_read_block(bgd[0].bg_block_bitmap, bmap);
   uint32_t bit_index = blk - 1;
   bmap[bit_index / 8] &= ~(1 << (bit_index % 8));
   ext2_write_block(bgd[0].bg_block_bitmap, bmap);
   bgd[0].bg_free_blocks_count++;
   ext2_write_block(bgd_block, bgd_buf);
}

static void ext2_free_inode_record(uint32_t inode_no) {
   if (inode_no == 0) {
      return;
   }// if
   uint32_t bgd_block = (ext2_block_size == 1024) ? 2 : 1;
   static uint8_t bgd_buf[4096];
   ext2_read_block(bgd_block, bgd_buf);
   struct ext2_group_desc *bgd = (struct ext2_group_desc *)bgd_buf;
   static uint8_t imap[4096];
   ext2_read_block(bgd[0].bg_inode_bitmap, imap);
   uint32_t bit_index = inode_no - 1;
   imap[bit_index / 8] &= ~(1 << (bit_index % 8));
   ext2_write_block(bgd[0].bg_inode_bitmap, imap);
   bgd[0].bg_free_inodes_count++;
   ext2_write_block(bgd_block, bgd_buf);
}

int ext2_remove(const char *path) {
   if (path == NULL || *path == '\0') {
      return -1;
   }
   uint32_t last_slash = -1;
   for (uint32_t i = 0; path[i] != '\0'; i++) {
      if (path[i] == '/') {
         last_slash = i;
      }
   }
   char parent_path[256];
   if (last_slash != (uint32_t)-1 && last_slash >= sizeof(parent_path)) {
      return -1;
   }
   const char *filename;
   if (last_slash == (uint32_t) -1) {
      parent_path[0] = '/';
      parent_path[1] = '\0';
      filename = path;
   } else if (last_slash == 0) {
      parent_path[0] = '/';
      parent_path[1] = '\0';
      filename = path + 1;
   } else {
      for (uint32_t i = 0; i < last_slash; i++) {
         parent_path[i] = path[i];
      }
      parent_path[last_slash] = '\0';
      filename = path + last_slash + 1;
   }
   uint32_t name_len = 0;
   while (filename[name_len] != '\0') {
      name_len++;
   }
   if (name_len == 0) {
      return -1;
   }
   uint32_t dir_ino = ext2_resolve_path(parent_path);
   if (dir_ino == 0) {
      return -1;
   }
   struct ext2_inode dir_inode;
   if (ext2_read_inode(dir_ino, &dir_inode) != 0) {
      return -1;
   }
   static uint8_t dir_buffer[4096];
   uint32_t target_ino = 0;
   bool entry_removed = false;
   for (uint32_t i = 0; i < 12 && !entry_removed; i++) {
      uint32_t blk = dir_inode.inode_block[i];
      if (blk == 0) {
         continue;
      }
      ext2_read_block(blk, dir_buffer);
      uint32_t offset = 0;
      struct ext2_dir_entry *prev = NULL;
      while (offset < ext2_block_size) {
         struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(dir_buffer + offset);
         if (entry->rec_len == 0) {
            break;
         }
         if (dir_entry_matches(entry, filename, name_len)) {
            target_ino = entry->inode;
            if (prev != NULL) {
               prev->rec_len += entry->rec_len;
            } else {
               entry->inode = 0;
            }
            ext2_write_block(blk, dir_buffer);
            entry_removed = true;
            break;
         }
         prev = entry;
         offset += entry->rec_len;
      }
   }
   if (!entry_removed || target_ino == 0) {
      return -1;
   }
   struct ext2_inode target_inode;
   if (ext2_read_inode(target_ino, &target_inode) != 0) {
      return -1;
   }
   if (target_inode.inode_links_count > 0) {
      target_inode.inode_links_count--;
   }
   if (target_inode.inode_links_count == 0) {
      for (uint32_t i = 0; i < 12; i++) {
         if (target_inode.inode_block[i] != 0) {
            ext2_free_block(target_inode.inode_block[i]);
            target_inode.inode_block[i] = 0;
         }
      }
      ext2_free_inode_record(target_ino);
      target_inode.inode_dtime = 1;
   }
   ext2_write_inode(target_ino, &target_inode);
   return 0;
}

void ext2_driver_process(void) {
   init_ext2();
   struct fs_ipc_req req;
   uint32_t status;
   while (1) {
      uint32_t sender = user_ipc_recv(0, &req);
      if (req.cmd == 2) {
         status = ext2_mkdir(req.path);
      }// if
      if (sender != (uint32_t)-1) {
        user_ipc_send(sender, (void *)(uintptr_t)status);
      }// if
   }// while
}// ext2_driver_process

int ext2_virtio_transfer(uint32_t sector, void *buffer, bool is_write) {
   uint32_t virtio_pid = user_get_pid_by_gid(VIRTIO_GID);
   if (virtio_pid == (uint32_t)-1) {
      return -1;
   }// if
   struct virtio_ipc_req req;
   req.cmd = is_write ? 1 : 0;
   req.sector = sector;
   req.buffer = buffer;
   uint32_t status = 0;
   user_ipc_call(virtio_pid, &req, &status);
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

static uint32_t ext2_alloc_inode(void) {
   uint32_t bgd_block = (ext2_block_size == 1024) ? 2 : 1;
   static uint8_t bgd_buf[4096];
   ext2_read_block(bgd_block, bgd_buf);
   struct ext2_group_desc *bgd = (struct ext2_group_desc *)bgd_buf;
   if (bgd[0].bg_free_inodes_count == 0) {
      return 0;
   }// if
   static uint8_t bmap[4096];
   ext2_read_block(bgd[0].bg_inode_bitmap, bmap);
   for (uint32_t byte = 0; byte < ext2_block_size; byte++) {
      if (bmap[byte] != 0xFF) {
         for (uint8_t bit = 0; bit < 8; bit++) {
            if ((bmap[byte] & (1 << bit)) == 0) {
               bmap[byte] |= (1 << bit);
               ext2_write_block(bgd[0].bg_inode_bitmap, bmap);
               bgd[0].bg_free_inodes_count--;
               ext2_write_block(bgd_block, bgd_buf);
               uint32_t new_inode = byte * 8 + bit + 1;
               return new_inode;
            }// if
         }// for
      }//if
   }// for
   return 0;
}// ext2_alloc_inode

uint32_t ext2_mkdir(const char *path) {
   if (path == NULL || *path == '\0') {
      return -1;
   }// if  
   uint32_t last_slash = -1;
   for (uint32_t i = 0; path[i] != '\0'; i++) {
      if (path[i] == '/') {
         last_slash = i;
      }// if
   }// for
   char parent_path[256];
   if (last_slash != (uint32_t)-1 && last_slash >= sizeof(parent_path)) {
      return -1;
   }
   const char *filename;
   if (last_slash == (uint32_t) -1) {
      parent_path[0] = '/';
      parent_path[1] = '\0';
      filename = path;
   } else if (last_slash == 0) {
      parent_path[0] = '/';
      parent_path[1] = '\0';
      filename = path + 1;
   } else {
      for (uint32_t i = 0; i < last_slash; i++) {
         parent_path[i] = path[i];
      }// for
      parent_path[last_slash] = '\0';
      filename = path + last_slash + 1;
   }// if
   uint32_t name_len = 0;
   while (filename[name_len] != '\0') {
      name_len++;
   }// while
   if (name_len == 0) {
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
   if (ext2_lookup(parent_ino, filename) != 0) {
      return 0; // already exists
   }// if
   uint32_t new_ino = ext2_alloc_inode();
   uint32_t new_blk = ext2_alloc_block();
   if (new_ino == 0 || new_blk == 0) {
      return -1;
   }// if
   struct ext2_inode new_inode;
   memset(&new_inode, 0, sizeof(struct ext2_inode));
   new_inode.inode_mode = 0x41ED;
   new_inode.inode_size = ext2_block_size;
   new_inode.inode_links_count = 2;
   new_inode.inode_blocks = ext2_block_size / 512;
   new_inode.inode_block[0] = new_blk;
   ext2_write_inode(new_ino, &new_inode);
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
   ext2_write_block(new_blk, dir_buf);    
   static uint8_t parent_buf[4096];
   ext2_read_block(parent_inode.inode_block[0], parent_buf);
   uint32_t offset = 0;
   struct ext2_dir_entry *entry = (struct ext2_dir_entry *)parent_buf;
   while (offset + entry->rec_len < ext2_block_size) {
      offset += entry->rec_len;
      entry = (struct ext2_dir_entry *)(parent_buf + offset);
   }// while
   uint32_t actual_last_len = ((8 + entry->name_len + 3) / 4) * 4;
   uint32_t new_entry_len = entry->rec_len - actual_last_len;
   entry->rec_len = actual_last_len;
   struct ext2_dir_entry *new_entry = (struct ext2_dir_entry *)((uint8_t *)entry + actual_last_len);
   new_entry->inode = new_ino;
   new_entry->rec_len = new_entry_len;
   new_entry->name_len = (uint8_t)name_len;
   new_entry->file_type = 2;
   memcpy(new_entry->name, filename, name_len);
   ext2_write_block(parent_inode.inode_block[0], parent_buf);
   parent_inode.inode_links_count++;
   ext2_write_inode(parent_ino, &parent_inode);
   return 0;
}// ext2_mkdir

uint32_t ext2_create_file(const char *path) {
   if (path == NULL || *path == '\0') {
      return -1;
   }// if  
   uint32_t last_slash = -1;
   for (uint32_t i = 0; path[i] != '\0'; i++) {
      if (path[i] == '/') {
         last_slash = i;
      }// if
   }// for
   char parent_path[256];
   if (last_slash != (uint32_t)-1 && last_slash >= sizeof(parent_path)) {
      return -1;
   }
   const char *filename;
   if (last_slash == (uint32_t) -1) {
      parent_path[0] = '/';
      parent_path[1] = '\0';
      filename = path;
   } else if (last_slash == 0) {
      parent_path[0] = '/';
      parent_path[1] = '\0';
      filename = path + 1;
   } else {
      for (uint32_t i = 0; i < last_slash; i++) {
         parent_path[i] = path[i];
      }// for
      parent_path[last_slash] = '\0';
      filename = path + last_slash + 1;
   }// if
   uint32_t name_len = 0;
   while (filename[name_len] != '\0') {
      name_len++;
   }// while
   if (name_len == 0) {
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
   if (ext2_lookup(parent_ino, filename) != 0) {
      return 0; // already exists
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
   ext2_write_inode(new_ino, &new_inode);  
   static uint8_t parent_buf[4096];
   ext2_read_block(parent_inode.inode_block[0], parent_buf);
   uint32_t offset = 0;
   struct ext2_dir_entry *entry = (struct ext2_dir_entry *)parent_buf;
   while (offset + entry->rec_len < ext2_block_size) {
      offset += entry->rec_len;
      entry = (struct ext2_dir_entry *)(parent_buf + offset);
   }// while
   uint32_t actual_last_len = ((8 + entry->name_len + 3) / 4) * 4;
   uint32_t new_entry_len = entry->rec_len - actual_last_len;
   entry->rec_len = actual_last_len;
   struct ext2_dir_entry *new_entry = (struct ext2_dir_entry *)((uint8_t *)entry + actual_last_len);
   new_entry->inode = new_ino;
   new_entry->rec_len = new_entry_len;
   new_entry->name_len = (uint8_t)name_len;
   new_entry->file_type = 1;
   memcpy(new_entry->name, filename, name_len);
   ext2_write_block(parent_inode.inode_block[0], parent_buf);
   ext2_write_inode(parent_ino, &parent_inode);
   return 0;
}// ext2_mkdir
