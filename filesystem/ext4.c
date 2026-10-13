#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../include/kernel.h"
#include "../include/drivers.h"
#include "../include/userspace.h"
#include "../include/gid.h"

#define EXT4_SUPER_MAGIC 0xEF53
#define EXT4_EXTENTS_MAGIC 0xF30A
#define EXT4_ROOT_INO 2

#define EXT4_FEATURE_INCOMPAT_EXTENTS 0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT 0x0080
#define EXT4_EXTENTS_FL 0x00080000

#define EXT4_MAX_EXTENT_DEPTH 5
#define EXT4_MAX_PATH_DEPTH 128

struct ext4_super_block {
   uint32_t s_inodes_count;
   uint32_t s_blocks_count_lo;
   uint32_t s_r_blocks_count_lo;
   uint32_t s_free_blocks_count_lo;
   uint32_t s_free_inodes_count;
   uint32_t s_first_data_block;
   uint32_t s_log_block_size;
   uint32_t s_log_cluster_size;
   uint32_t s_blocks_per_group;
   uint32_t s_clusters_per_group;
   uint32_t s_inodes_per_group;
   uint32_t s_mtime;
   uint32_t s_wtime;
   uint16_t s_mnt_count;
   uint16_t s_max_mnt_count;
   uint16_t s_magic;
   uint16_t s_state;
   uint16_t s_errors;
   uint16_t s_minor_rev_level;
   uint32_t s_lastcheck;
   uint32_t s_checkinterval;
   uint32_t s_creator_os;
   uint32_t s_rev_level;
   uint16_t s_def_resuid;
   uint16_t s_def_resgid;
   uint32_t s_first_ino;
   uint16_t s_inode_size;
   uint16_t s_block_group_nr;
   uint32_t s_feature_compat;
   uint32_t s_feature_incompat;
   uint32_t s_feature_ro_compat;
   uint8_t  s_uuid[16];
   char     s_volume_name[16];
} __attribute__((packed));

struct ext4_group_desc {
   uint32_t bg_block_bitmap_lo;
   uint32_t bg_inode_bitmap_lo;
   uint32_t bg_inode_table_lo;
   uint16_t bg_free_blocks_count_lo;
   uint16_t bg_free_inodes_count_lo;
   uint16_t bg_used_dirs_count_lo;
   uint16_t bg_flags;
   uint32_t bg_exclude_bitmap_lo;
   uint16_t bg_block_bitmap_csum_lo;
   uint16_t bg_inode_bitmap_csum_lo;
   uint16_t bg_itable_unused_lo;
   uint16_t bg_checksum;
   uint32_t bg_block_bitmap_hi;
   uint32_t bg_inode_bitmap_hi;
   uint32_t bg_inode_table_hi;
   uint16_t bg_free_blocks_count_hi;
   uint16_t bg_free_inodes_count_hi;
   uint16_t bg_used_dirs_count_hi;
   uint16_t bg_itable_unused_hi;
   uint32_t bg_exclude_bitmap_hi;
   uint16_t bg_block_bitmap_csum_hi;
   uint16_t bg_inode_bitmap_csum_hi;
   uint32_t bg_reserved;
} __attribute__((packed));

struct ext4_extent_header {
   uint16_t eh_magic;
   uint16_t eh_entries;
   uint16_t eh_max;
   uint16_t eh_depth;
   uint32_t eh_generation;
} __attribute__((packed));

struct ext4_extent {
   uint32_t ee_block;
   uint16_t ee_len;
   uint16_t ee_start_hi;
   uint32_t ee_start_lo;
} __attribute__((packed));

struct ext4_extent_idx {
   uint32_t ei_block;
   uint32_t ei_leaf_lo;
   uint16_t ei_leaf_hi;
   uint16_t ei_unused;
} __attribute__((packed));

struct ext4_inode {
   uint16_t i_mode;
   uint16_t i_uid;
   uint32_t i_size_lo;
   uint32_t i_atime;
   uint32_t i_ctime;
   uint32_t i_mtime;
   uint32_t i_dtime;
   uint16_t i_gid;
   uint16_t i_links_count;
   uint32_t i_blocks_lo;
   uint32_t i_flags;
   uint32_t i_osd1;
   uint32_t i_block[15];
   uint32_t i_generation;
   uint32_t i_file_acl_lo;
   uint32_t i_size_high;
   uint32_t i_obso_faddr;
} __attribute__((packed));

struct ext4_dir_entry_2 {
   uint32_t inode;
   uint16_t rec_len;
   uint8_t  name_len;
   uint8_t  file_type;
   char     name[];
} __attribute__((packed));

static uint32_t ext4_block_size = 1024;
static uint32_t ext4_inode_size = 256;
static uint32_t ext4_inodes_per_group = 0;
static uint32_t ext4_blocks_per_group = 0;
static uint32_t ext4_first_data_block = 1;
static uint32_t ext4_desc_size = 32;
static uint32_t ext4_total_groups = 0;
static uint32_t ext4_blocks_count = 0;
static uint32_t ext4_inodes_count = 0;
static bool     ext4_initialized = false;

extern int virtio_read_sector(uint64_t sector, void *buffer);

int ext4_read_block(uint64_t block, void *buffer) {
   if (!buffer || ext4_block_size < 512 || ext4_block_size > 4096) {
      return -1;
   }// if
   if (ext4_blocks_count != 0 && block >= ext4_blocks_count) {
      memset(buffer, 0, ext4_block_size);
      return -1;
   }// if
   uint32_t sectors_per_block = ext4_block_size / 512;
   if (sectors_per_block == 0 || block > (0xFFFFFFFFFFFFFFFFULL - sectors_per_block) / sectors_per_block) {
      memset(buffer, 0, ext4_block_size);
      return -1;
   }// if
   uint64_t start_sector = block * sectors_per_block;
   for (uint32_t i = 0; i < sectors_per_block; i++) {
      if (virtio_read_sector(start_sector + i, (uint8_t *)buffer + (i * 512)) != 0) {
         errorn("ext4: sector read error");
         memset(buffer, 0, ext4_block_size);
         return -1;
      }// if
   }// for
   return 0;
}// ext4_read_block

static int ext4_read_group_desc(uint32_t group, struct ext4_group_desc *desc) {
   if (!desc || !ext4_initialized || ext4_total_groups == 0 || group >= ext4_total_groups || ext4_block_size == 0) {
      return -1;
   }// if
   uint32_t desc_size = ext4_desc_size;
   if (desc_size < 32 || desc_size > ext4_block_size) {
      return -1;
   }// if
   uint64_t desc_offset = (uint64_t)group * desc_size;
   uint64_t block_offset = desc_offset / ext4_block_size;
   uint32_t byte_offset = (uint32_t)(desc_offset % ext4_block_size);
   if (byte_offset + desc_size > ext4_block_size) {
      return -1;
   }// if
   uint64_t bgd_block = (uint64_t)ext4_first_data_block + 1 + block_offset;
   static uint8_t buf[4096];
   if (ext4_read_block(bgd_block, buf) != 0) {
      return -1;
   }// if
   memset(desc, 0, sizeof(struct ext4_group_desc));
   uint32_t to_copy = desc_size;
   if (to_copy > sizeof(struct ext4_group_desc)) {
      to_copy = sizeof(struct ext4_group_desc);
   }// if
   memcpy(desc, buf + byte_offset, to_copy);
   return 0;
}// ext4_read_group_desc

int ext4_read_inode(uint32_t inode_no, struct ext4_inode *out) {
   if (inode_no == 0 || !out || !ext4_initialized || ext4_inodes_per_group == 0 || ext4_block_size == 0) {
      return -1;
   }// if
   if (ext4_inodes_count != 0 && inode_no > ext4_inodes_count) {
      return -1;
   }// if

   uint32_t group = (inode_no - 1) / ext4_inodes_per_group;
   uint32_t index = (inode_no - 1) % ext4_inodes_per_group;
   if (ext4_total_groups != 0 && group >= ext4_total_groups) {
      return -1;
   }// if

   struct ext4_group_desc bgd;
   if (ext4_read_group_desc(group, &bgd) != 0) {
      return -1;
   }// if

   uint64_t itable_block = bgd.bg_inode_table_lo;
   if (ext4_desc_size >= 64) {
      itable_block |= ((uint64_t)bgd.bg_inode_table_hi << 32);
   }// if
   if (itable_block == 0) {
      return -1;
   }// if

   uint64_t inode_offset = (uint64_t)index * ext4_inode_size;
   uint64_t inode_block = itable_block + (inode_offset / ext4_block_size);
   uint32_t byte_offset = (uint32_t)(inode_offset % ext4_block_size);

   uint32_t to_copy = sizeof(struct ext4_inode);
   if (to_copy > ext4_inode_size) {
      to_copy = ext4_inode_size;
   }// if
   if (byte_offset + to_copy > ext4_block_size) {
      return -1;
   }// if

   static uint8_t buf[4096];
   if (ext4_read_block(inode_block, buf) != 0) {
      memset(out, 0, sizeof(struct ext4_inode));
      return -1;
   }// if

   memset(out, 0, sizeof(struct ext4_inode));
   memcpy(out, buf + byte_offset, to_copy);
   return 0;
}// ext4_read_inode

static int ext4_find_leaf_block(const uint8_t *block_data, uint32_t block_offset, uint64_t *phys_block) {
   if (!block_data || !phys_block) {
      return -1;
   }// if
   *phys_block = 0;

   static uint8_t buf[4096];
   const uint8_t *curr = block_data;
   const struct ext4_extent_header *eh = (const struct ext4_extent_header *)curr;

   if (eh->eh_magic != EXT4_EXTENTS_MAGIC) {
      return -1;
   }// if
   if (eh->eh_depth > EXT4_MAX_EXTENT_DEPTH) {
      return -1;
   }// if

   uint16_t root_max = (sizeof(((struct ext4_inode *)0)->i_block) - sizeof(struct ext4_extent_header)) / sizeof(struct ext4_extent);
   if (eh->eh_max == 0 || eh->eh_max > root_max || eh->eh_entries > eh->eh_max) {
      return -1;
   }// if

   uint64_t visited[6];
   uint32_t visited_count = 0;
   uint16_t expected_depth = eh->eh_depth;

   while (eh->eh_depth > 0) {
      const struct ext4_extent_idx *idx = (const struct ext4_extent_idx *)(curr + sizeof(struct ext4_extent_header));
      if (eh->eh_entries == 0 || block_offset < idx[0].ei_block) {
         return -1;
      }// if
      for (uint16_t i = 1; i < eh->eh_entries; i++) {
         if (idx[i].ei_block <= idx[i - 1].ei_block) {
            return -1;
         }// if
      }// for
      bool found = false;
      uint64_t next_leaf = 0;
      for (uint16_t i = 0; i < eh->eh_entries; i++) {
         if (i == eh->eh_entries - 1 || block_offset < idx[i + 1].ei_block) {
            next_leaf = ((uint64_t)idx[i].ei_leaf_hi << 32) | idx[i].ei_leaf_lo;
            found = true;
            break;
         }// if
      }// for
      if (!found || next_leaf == 0) {
         return -1;
      }// if
      if (ext4_blocks_count != 0 && next_leaf >= ext4_blocks_count) {
         return -1;
      }// if
      for (uint32_t v = 0; v < visited_count; v++) {
         if (visited[v] == next_leaf) {
            return -1;
         }// if
      }// for
      if (visited_count < 6) {
         visited[visited_count++] = next_leaf;
      }// if
      if (ext4_read_block(next_leaf, buf) != 0) {
         return -1;
      }// if
      curr = buf;
      eh = (const struct ext4_extent_header *)curr;
      if (eh->eh_magic != EXT4_EXTENTS_MAGIC) {
         return -1;
      }// if
      expected_depth--;
      if (eh->eh_depth != expected_depth) {
         return -1;
      }// if
      uint32_t max_block_entries = (ext4_block_size - sizeof(struct ext4_extent_header)) / sizeof(struct ext4_extent);
      if (eh->eh_max == 0 || eh->eh_max > max_block_entries || eh->eh_entries > eh->eh_max) {
         return -1;
      }// if
   }// while

   const struct ext4_extent *ext = (const struct ext4_extent *)(curr + sizeof(struct ext4_extent_header));
   for (uint16_t i = 0; i < eh->eh_entries; i++) {
      uint16_t len = ext[i].ee_len;
      bool unwritten = false;
      if (len > 32768) {
         len -= 32768;
         unwritten = true;
      }// if
      if (len == 0) {
         return -1;
      }// if
      uint64_t ee_end = (uint64_t)ext[i].ee_block + len;
      if (ee_end > 0x100000000ULL) {
         return -1;
      }// if
      if (i > 0) {
         uint16_t prev_len = ext[i - 1].ee_len;
         if (prev_len > 32768) {
            prev_len -= 32768;
         }// if
         uint64_t prev_end = (uint64_t)ext[i - 1].ee_block + prev_len;
         if (ext[i].ee_block < prev_end) {
            return -1;
         }// if
         if (ext[i].ee_block <= ext[i - 1].ee_block) {
            return -1;
         }// if
      }// if
      if (block_offset >= ext[i].ee_block && block_offset < ee_end) {
         if (unwritten) {
            *phys_block = 0;
         } else {
            uint64_t start = ((uint64_t)ext[i].ee_start_hi << 32) | ext[i].ee_start_lo;
            if (start == 0 || (ext4_blocks_count != 0 && start + len > ext4_blocks_count)) {
               return -1;
            }// if
            *phys_block = start + (block_offset - ext[i].ee_block);
         }// else
         return 0;
      }// if
   }// for

   return -1;
}// ext4_find_leaf_block

int ext4_map_file_block(const struct ext4_inode *inode, uint32_t block_offset, uint64_t *phys_block) {
   if (!inode || !phys_block) {
      return -1;
   }// if
   *phys_block = 0;

   if (!(inode->i_flags & EXT4_EXTENTS_FL) &&
       ((const struct ext4_extent_header *)inode->i_block)->eh_magic != EXT4_EXTENTS_MAGIC) {
      if (block_offset < 12) {
         uint32_t blk = inode->i_block[block_offset];
         if (ext4_blocks_count != 0 && blk >= ext4_blocks_count) {
            return -1;
         }// if
         *phys_block = blk;
         return 0;
      }// if
      return -1;
   }// if

   return ext4_find_leaf_block((const uint8_t *)inode->i_block, block_offset, phys_block);
}// ext4_map_file_block

int ext4_read_extent_file(const struct ext4_inode *inode, uint32_t block_offset, uint32_t *phys_block) {
   if (!inode || !phys_block) {
      return -1;
   }// if
   *phys_block = 0;
   uint64_t pb = 0;
   int ret = ext4_map_file_block(inode, block_offset, &pb);
   if (ret == 0) {
      if (pb > 0xFFFFFFFFULL) {
         return -1;
      }// if
      *phys_block = (uint32_t)pb;
   }// if
   return ret;
}// ext4_read_extent_file

static uint32_t ext4_lookup(uint32_t dir_ino, const char *name, uint32_t name_len) {
   if (!name || name_len == 0 || name_len > 255 || !ext4_initialized) {
      return 0;
   }// if
   struct ext4_inode inode;
   if (ext4_read_inode(dir_ino, &inode) != 0 || (inode.i_mode & 0xF000) != 0x4000) {
      return 0;
   }// if

   uint64_t file_size = inode.i_size_lo;
   if (file_size == 0) {
      return 0;
   }// if
   uint64_t blocks_calc = (file_size + ext4_block_size - 1) / ext4_block_size;
   if (blocks_calc > 65536) {
      blocks_calc = 65536;
   }// if
   uint32_t total_blocks = (uint32_t)blocks_calc;
   static uint8_t dir_buf[4096];

   for (uint32_t b = 0; b < total_blocks; b++) {
      uint64_t phys_block = 0;
      if (ext4_map_file_block(&inode, b, &phys_block) != 0 || phys_block == 0) {
         continue;
      }// if
      if (ext4_read_block(phys_block, dir_buf) != 0) {
         continue;
      }// if

      uint32_t offset = 0;
      while (offset + sizeof(struct ext4_dir_entry_2) <= ext4_block_size) {
         struct ext4_dir_entry_2 *entry = (struct ext4_dir_entry_2 *)(dir_buf + offset);
         if ((offset & 3) != 0 || (entry->rec_len & 3) != 0) {
            break;
         }// if
         if (entry->rec_len < sizeof(struct ext4_dir_entry_2) || offset + entry->rec_len > ext4_block_size) {
            break;
         }// if
         if (entry->name_len > entry->rec_len - sizeof(struct ext4_dir_entry_2)) {
            break;
         }// if
         if (offset + sizeof(struct ext4_dir_entry_2) + entry->name_len > ext4_block_size) {
            break;
         }// if
         if (entry->inode != 0 && entry->name_len == name_len) {
            if (ext4_inodes_count == 0 || entry->inode <= ext4_inodes_count) {
               if (memcmp(entry->name, name, name_len) == 0) {
                  return entry->inode;
               }// if
            }// if
         }// if
         offset += entry->rec_len;
      }// while
   }// for

   return 0;
}// ext4_lookup

uint32_t ext4_resolve_path(const char *path) {
   if (!path || *path == '\0' || !ext4_initialized) {
      return 0;
   }// if
   uint32_t curr_ino = EXT4_ROOT_INO;
   uint32_t depth = 0;
   while (*path == '/') {
      path++;
   }// while
   if (*path == '\0') {
      return curr_ino;
   }// if

   while (*path != '\0') {
      if (++depth > EXT4_MAX_PATH_DEPTH) {
         return 0;
      }// if
      const char *next_slash = path;
      while (*next_slash != '\0' && *next_slash != '/') {
         next_slash++;
      }// while
      uint32_t len = (uint32_t)(next_slash - path);
      if (len == 0 || len > 255) {
         return 0;
      }// if

      curr_ino = ext4_lookup(curr_ino, path, len);
      if (curr_ino == 0) {
         return 0;
      }// if
      path = next_slash;
      while (*path == '/') {
         path++;
      }// while
   }// while

   return curr_ino;
}// ext4_resolve_path

int ext4_read_file(const char *path, void *buffer, uint32_t count, uint32_t offset) {
   if (!path || !buffer || !ext4_initialized || count == 0) {
      return -1;
   }// if
   if (count > 0x7FFFFFFF) {
      count = 0x7FFFFFFF;
   }// if
   uint32_t ino = ext4_resolve_path(path);
   if (ino == 0) {
      return -1;
   }// if

   struct ext4_inode inode;
   if (ext4_read_inode(ino, &inode) != 0) {
      return -1;
   }// if
   if ((inode.i_mode & 0xF000) == 0x4000) {
      return -1;
   }// if

   uint64_t file_size = ((uint64_t)inode.i_size_high << 32) | inode.i_size_lo;
   if ((uint64_t)offset >= file_size) {
      return 0;
   }// if
   if ((uint64_t)offset + count > file_size) {
      count = (uint32_t)(file_size - offset);
   }// if

   uint32_t bytes_read = 0;
   static uint8_t blk_buf[4096];

   while (bytes_read < count) {
      uint64_t curr_pos = (uint64_t)offset + bytes_read;
      uint32_t blk_idx = (uint32_t)(curr_pos / ext4_block_size);
      uint32_t blk_off = (uint32_t)(curr_pos % ext4_block_size);
      uint32_t to_copy = ext4_block_size - blk_off;
      if (to_copy > count - bytes_read) {
         to_copy = count - bytes_read;
      }// if

      uint64_t phys_block = 0;
      if (ext4_map_file_block(&inode, blk_idx, &phys_block) != 0 || phys_block == 0) {
         memset((uint8_t *)buffer + bytes_read, 0, to_copy);
      } else {
         if (ext4_read_block(phys_block, blk_buf) != 0) {
            return -1;
         }// if
         memcpy((uint8_t *)buffer + bytes_read, blk_buf + blk_off, to_copy);
      }// else
      bytes_read += to_copy;
   }// while

   return (int)bytes_read;
}// ext4_read_file

void init_ext4(void) {
   static uint8_t buf[1024];
   infon("Probing ext4 filesystem");

   if (virtio_read_sector(2, buf) != 0 || virtio_read_sector(3, buf + 512) != 0) {
      errorn("ext4: failed to read superblock sectors 2/3");
      return;
   }// if

   struct ext4_super_block *sb = (struct ext4_super_block *)buf;
   if (sb->s_magic == EXT4_SUPER_MAGIC) {
      if (sb->s_log_block_size > 2) {
         errorn("ext4: unsupported block size log (>2)");
         return;
      }// if
      ext4_block_size = 1024 << sb->s_log_block_size;
      if (ext4_block_size < 1024 || ext4_block_size > 4096) {
         errorn("ext4: invalid block size");
         return;
      }// if
      if (sb->s_blocks_per_group == 0 || sb->s_inodes_per_group == 0) {
         errorn("ext4: blocks or inodes per group is zero");
         return;
      }// if
      if (sb->s_blocks_count_lo == 0 || sb->s_inodes_count == 0) {
         errorn("ext4: total blocks or inodes is zero");
         return;
      }// if
      ext4_inode_size = (sb->s_rev_level >= 1 && sb->s_inode_size != 0) ? sb->s_inode_size : 256;
      if (ext4_inode_size < sizeof(struct ext4_inode) || ext4_inode_size > ext4_block_size) {
         errorn("ext4: invalid inode size");
         return;
      }// if
      if (sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) {
         ext4_desc_size = 64;
      } else {
         ext4_desc_size = 32;
      }// else
      ext4_inodes_per_group = sb->s_inodes_per_group;
      ext4_blocks_per_group = sb->s_blocks_per_group;
      ext4_first_data_block = sb->s_first_data_block;
      ext4_blocks_count = sb->s_blocks_count_lo;
      ext4_inodes_count = sb->s_inodes_count;
      uint64_t total_groups_calc = ((uint64_t)sb->s_blocks_count_lo + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
      if (total_groups_calc == 0 || total_groups_calc > 0xFFFFFFFFULL) {
         errorn("ext4: invalid group count calculation");
         return;
      }// if
      ext4_total_groups = (uint32_t)total_groups_calc;
      ext4_initialized = true;
      successn("ext4: valid superblock mounted successfully");
   } else {
      errorn("ext4: magic mismatch");
   }// else
}// init_ext4

bool ext4_check_magic(const void *sb_buf) {
   if (!sb_buf) {
      return false;
   }// if
   const struct ext4_super_block *sb = (const struct ext4_super_block *)sb_buf;
   return (sb->s_magic == EXT4_SUPER_MAGIC);
}// ext4_check_magic

bool ext4_is_mounted(void) {
   return ext4_initialized;
}// ext4_is_mounted

void ext4_driver_process(void) {
   init_ext4();
   struct fs_ipc_req req;
   uint32_t status;
   while (1) {
      uint32_t sender = user_ipc_recv(0, &req);
      if (sender == (uint32_t)-1) {
         user_yield();
         continue;
      }// if
      if (sender < MIN_PID || sender > MAX_PID) {
         continue;
      }// if
      req.path[sizeof(req.path) - 1] = '\0';
      if (!ext4_initialized) {
         status = (uint32_t)-1;
      } else if (req.cmd == 0) {
         uint32_t ino = ext4_resolve_path(req.path);
         status = (ino != 0) ? ino : (uint32_t)-1;
      } else if (req.cmd == 1) {
         status = (ext4_resolve_path(req.path) != 0) ? 0 : (uint32_t)-1;
      } else {
         status = (uint32_t)-1;
      }// else
      user_ipc_send(sender, (void *)(uintptr_t)status);
   }// while
}// ext4_driver_process

void create_ext4_process(void) {
   infon("Starting ext4 process");
   uint32_t pid = create_process((void *)ext4_driver_process, FS_GID);
   if (pid == (uint32_t)-1) {
      errorn("ext4: failed to create process");
      return;
   }// if
   struct process *ext4_process = get_process_by_pid(pid);
   if (ext4_process) {
      uint32_t gid = ext4_process->gid;
      uint32_t sid = generate_sid(gid, (uintptr_t)ext4_driver_process);
      ext4_process->sid = sid;
   }// if
   successn("ext4 process started");
}// create_ext4_process
