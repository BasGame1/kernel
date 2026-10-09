#include <stdint.h>

#include "../include/hardware.h"
#include "../include/kernel.h"

static uint32_t ext2_block_size = 1024;

void ext2_read_block(uint32_t block, void *buffer) {
   uint32_t sectors_per_block = ext2_block_size / 512;
   uint32_t start_sector = block * sectors_per_block;
   for (uint32_t i = 0; i < sectors_per_block; i++) {
      if (virtio_read_sector(start_sector + i, (uint8_t *)buffer + (i * 512)) != 0) {
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
      if (virtio_write_sector(start_sector + i, (uint8_t *)buffer + (i * 512)) != 0) {
         error("Write failed at sector ");
         print_dec(start_sector + i);
         print_character("\n");
         return;
      }// if check fail
   }// for loop to all the sectors
}// ext2_write_block

void init_ext2() {
   static uint8_t buffer[1024];
   if (virtio_read_sector(2, buffer) != 0 || virtio_read_sector(3, buffer + 512) != 0) {
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
   uint32_t inode_table_block = bgd[0].bg_inode_table;
   print_dec(inode_table_block);
   /* inodes */
   uint32_t inode_id = 2; // unnecesary for now
   uint32_t inode_index = inode_id - 1;
   uint32_t inode_block = inode_table_block + (inode_index * 128) / ext2_block_size;
   uint32_t byte_offset = (inode_index * 128) % ext2_block_size;
   ext2_read_block(inode_block, buffer);
   struct ext2_inode *root_inode = (struct ext2_inode *)(buffer + byte_offset);
   uint32_t dir_size = root_inode->inode_size;
   uint32_t block_number = root_inode->inode_block[0];
   static uint8_t dir_buffer[4096];
   ext2_read_block(root_inode->inode_block[0], dir_buffer);
   uint32_t offset = 0;
   while (offset < root_inode->inode_size && offset < ext2_block_size) {
      struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(dir_buffer + offset);
      if (entry->rec_len == 0) {
         infon("Exiting loop");
         break;
      }// if to exit loop
      if (entry->inode != 0) {
        char name_buf[256];
        for (uint8_t i = 0; i < entry->name_len; i++) {
            name_buf[i] = entry->name[i];
        }// for loop
      }// if to check if value is not 0
      entry->name[entry->name_len - 1] = entry->name[0]
      printn(entry->name[entry->name_len - 1]);
      offset += entry->rec_len;
   }// while
}// init_ext2
