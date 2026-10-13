#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../include/kernel.h"
#include "../include/gid.h"
#include "../include/drivers.h"

#define BTRFS_MAGIC 0x4D5F53665248425FULL
#define BTRFS_SUPER_INFO_OFFSET (64 * 1024)
#define BTRFS_ROOT_TREE_OBJECTID 1ULL
#define BTRFS_EXTENT_TREE_OBJECTID 2ULL
#define BTRFS_CHUNK_TREE_OBJECTID 3ULL
#define BTRFS_DEV_TREE_OBJECTID 4ULL
#define BTRFS_FS_TREE_OBJECTID 5ULL
#define BTRFS_ROOT_TREE_DIR_OBJECTID 6ULL
#define BTRFS_CSUM_TREE_OBJECTID 7ULL
#define BTRFS_FIRST_FREE_OBJECTID 256ULL
#define BTRFS_INODE_ITEM_KEY 1
#define BTRFS_INODE_REF_KEY 12
#define BTRFS_DIR_ITEM_KEY 84
#define BTRFS_DIR_INDEX_KEY 96
#define BTRFS_EXTENT_DATA_KEY 108
#define BTRFS_ROOT_ITEM_KEY 132
#define BTRFS_CHUNK_ITEM_KEY 228
#define BTRFS_FILE_EXTENT_INLINE 0
#define BTRFS_FILE_EXTENT_REG 1
#define BTRFS_FILE_EXTENT_PREALLOC 2
#define BTRFS_MAX_SYS_CHUNKS 64
#define BTRFS_MAX_PATH_LEN 256
#define BTRFS_MAX_NODE_SIZE 16384
#define BTRFS_MAX_LEVEL 8
#define BTRFS_MAX_TRAVERSAL_DEPTH 32

struct btrfs_disk_key {
   uint64_t objectid;
   uint8_t type;
   uint64_t offset;
} __attribute__((packed));

struct btrfs_header {
   uint8_t csum[32];
   uint8_t fsid[16];
   uint64_t bytenr;
   uint64_t flags;
   uint8_t chunk_tree_uuid[16];
   uint64_t generation;
   uint64_t owner;
   uint32_t nritems;
   uint8_t level;
} __attribute__((packed));

struct btrfs_item {
   struct btrfs_disk_key key;
   uint32_t offset;
   uint32_t size;
} __attribute__((packed));

struct btrfs_key_ptr {
   struct btrfs_disk_key key;
   uint64_t blockptr;
   uint64_t generation;
} __attribute__((packed));

struct btrfs_stripe {
   uint64_t devid;
   uint64_t offset;
   uint8_t dev_uuid[16];
} __attribute__((packed));

struct btrfs_chunk {
   uint64_t length;
   uint64_t owner;
   uint64_t stripe_len;
   uint64_t type;
   uint32_t io_align;
   uint32_t io_width;
   uint32_t sector_size;
   uint16_t num_stripes;
   uint16_t sub_stripes;
   struct btrfs_stripe stripe;
} __attribute__((packed));

struct btrfs_inode_item {
   uint64_t generation;
   uint64_t transid;
   uint64_t size;
   uint64_t nbytes;
   uint64_t block_group;
   uint32_t nlink;
   uint32_t uid;
   uint32_t gid;
   uint32_t mode;
   uint64_t rdev;
   uint64_t flags;
   uint64_t sequence;
   uint8_t reserved[32];
} __attribute__((packed));

struct btrfs_dir_item {
   struct btrfs_disk_key location;
   uint64_t transid;
   uint16_t data_len;
   uint16_t name_len;
   uint8_t type;
} __attribute__((packed));

struct btrfs_file_extent_item {
   uint64_t generation;
   uint64_t ram_bytes;
   uint8_t compression;
   uint8_t encryption;
   uint16_t other_encoding;
   uint8_t type;
   union {
      struct {
         uint64_t disk_bytenr;
         uint64_t disk_num_bytes;
         uint64_t offset;
         uint64_t num_bytes;
      } __attribute__((packed)) reg;
      uint8_t inline_data[1];
   };
} __attribute__((packed));

struct btrfs_root_item {
   struct btrfs_inode_item inode;
   uint64_t generation;
   uint64_t root_dirid;
   uint64_t bytenr;
   uint64_t byte_limit;
   uint64_t bytes_used;
   uint64_t last_snapshot;
   uint64_t flags;
   uint32_t refs;
   struct btrfs_disk_key drop_progress;
   uint8_t drop_level;
   uint8_t level;
} __attribute__((packed));

struct btrfs_super_block {
   uint8_t csum[32];
   uint8_t fsid[16];
   uint64_t bytenr;
   uint64_t flags;
   uint64_t magic;
   uint64_t generation;
   uint64_t root;
   uint64_t chunk_root;
   uint64_t log_root;
   uint64_t log_root_transid;
   uint64_t total_bytes;
   uint64_t bytes_used;
   uint64_t root_dir_objectid;
   uint64_t num_devices;
   uint32_t sectorsize;
   uint32_t nodesize;
   uint32_t leafsize;
   uint32_t stripesize;
   uint32_t sys_chunk_array_size;
   uint64_t chunk_root_generation;
   uint64_t compat_flags;
   uint64_t compat_ro_flags;
   uint64_t incompat_flags;
   uint16_t csum_type;
   uint8_t root_level;
   uint8_t chunk_root_level;
   uint8_t log_root_level;
   uint8_t dev_item[98];
   char label[256];
   uint64_t cache_generation;
   uint64_t uuid_tree_generation;
   uint8_t reserved[240];
   uint8_t sys_chunk_array[2048];
} __attribute__((packed));

struct btrfs_chunk_map {
   uint64_t logical_start;
   uint64_t length;
   uint64_t physical_offset;
};

static struct btrfs_chunk_map sys_chunks[BTRFS_MAX_SYS_CHUNKS];
static size_t num_sys_chunks = 0;

static bool btrfs_mounted = false;
static uint32_t btrfs_nodesize = 4096;
static uint32_t btrfs_sectorsize = 4096;
static uint64_t btrfs_fs_tree_root = 0;
static uint8_t btrfs_fs_tree_level = 0;

extern int virtio_read_sector(uint64_t sector, void *buffer);

static int btrfs_add_chunk(uint64_t logical_start, uint64_t length, uint64_t physical_offset) {
   if (length == 0) {
      return -1;
   }// if
   if (logical_start > UINT64_MAX - length) {
      return -1;
   }// if
   if (physical_offset > UINT64_MAX - length) {
      return -1;
   }// if
   uint64_t logical_end = logical_start + length;
   for (size_t i = 0; i < num_sys_chunks; i++) {
      if (sys_chunks[i].logical_start == logical_start &&
          sys_chunks[i].length == length &&
          sys_chunks[i].physical_offset == physical_offset) {
         return 0;
      }// if
      uint64_t existing_end = sys_chunks[i].logical_start + sys_chunks[i].length;
      if (logical_start < existing_end && sys_chunks[i].logical_start < logical_end) {
         errorn("btrfs: corrupted or overlapping chunk map detected");
         return -1;
      }// if
   }// for
   if (num_sys_chunks >= BTRFS_MAX_SYS_CHUNKS) {
      return -1;
   }// if
   sys_chunks[num_sys_chunks].logical_start = logical_start;
   sys_chunks[num_sys_chunks].length = length;
   sys_chunks[num_sys_chunks].physical_offset = physical_offset;
   num_sys_chunks++;
   return 0;
}// btrfs_add_chunk

static int btrfs_logical_to_physical(uint64_t logical, uint64_t *physical_sector) {
   for (size_t i = 0; i < num_sys_chunks; i++) {
      if (logical >= sys_chunks[i].logical_start) {
         uint64_t delta = logical - sys_chunks[i].logical_start;
         if (delta < sys_chunks[i].length) {
            if (sys_chunks[i].physical_offset > UINT64_MAX - delta) {
               return -1;
            }// if
            uint64_t phys_bytes = sys_chunks[i].physical_offset + delta;
            if (physical_sector) {
               *physical_sector = phys_bytes / 512;
            }// if
            return 0;
         }// if
      }// if
   }// for
   errorn("btrfs: logical address not found in chunk map");
   return -1;
}// btrfs_logical_to_physical

static int btrfs_logical_to_sector(uint64_t logical, uint64_t *out_sector) {
   return btrfs_logical_to_physical(logical, out_sector);
}// btrfs_logical_to_sector

static int btrfs_read_node(uint64_t logical, void *buf, uint32_t size) {
   if (!buf || size == 0 || size > BTRFS_MAX_NODE_SIZE || logical == 0) {
      return -1;
   }// if
   uint64_t sector = 0;
   if (btrfs_logical_to_sector(logical, &sector) != 0) {
      return -1;
   }// if
   uint32_t num_sectors = (size + 511) / 512;
   for (uint32_t s = 0; s < num_sectors; s++) {
      if (sector > UINT64_MAX - s) {
         return -1;
      }// if
      if (virtio_read_sector(sector + s, (uint8_t *)buf + (s * 512)) != 0) {
         errorn("btrfs: sector read failure");
         return -1;
      }// if
   }// for
   return 0;
}// btrfs_read_node

static int btrfs_key_compare(const struct btrfs_disk_key *k1, uint64_t obj, uint8_t type, uint64_t off) {
   if (!k1) {
      return -1;
   }// if
   if (k1->objectid < obj) {
      return -1;
   }// if
   if (k1->objectid > obj) {
      return 1;
   }// if
   if (k1->type < type) {
      return -1;
   }// if
   if (k1->type > type) {
      return 1;
   }// if
   if (k1->offset < off) {
      return -1;
   }// if
   if (k1->offset > off) {
      return 1;
   }// if
   return 0;
}// btrfs_key_compare

static void btrfs_parse_sys_chunks(const struct btrfs_super_block *sb) {
   num_sys_chunks = 0;
   const uint8_t *cur = sb->sys_chunk_array;
   uint32_t array_size = sb->sys_chunk_array_size;
   if (array_size > sizeof(sb->sys_chunk_array)) {
      array_size = sizeof(sb->sys_chunk_array);
   }// if
   const uint8_t *end = cur + array_size;

   while (cur < end) {
      uint32_t remaining = (uint32_t)(end - cur);
      if (sizeof(struct btrfs_disk_key) + sizeof(struct btrfs_chunk) > remaining) {
         break;
      }// if
      const struct btrfs_disk_key *k = (const struct btrfs_disk_key *)cur;
      cur += sizeof(struct btrfs_disk_key);
      const struct btrfs_chunk *chunk = (const struct btrfs_chunk *)cur;

      if (chunk->num_stripes == 0) {
         break;
      }// if
      if (k->type == BTRFS_CHUNK_ITEM_KEY) {
         if (btrfs_add_chunk(k->offset, chunk->length, chunk->stripe.offset) != 0) {
            break;
         }// if
      }// if
      uint32_t add_stripes = chunk->num_stripes > 1 ? (uint32_t)(chunk->num_stripes - 1) : 0;
      if (add_stripes > (UINT32_MAX - sizeof(struct btrfs_chunk)) / sizeof(struct btrfs_stripe)) {
         break;
      }// if
      uint32_t advance = sizeof(struct btrfs_chunk) + add_stripes * sizeof(struct btrfs_stripe);
      if (advance > (uint32_t)(end - cur)) {
         break;
      }// if
      cur += advance;
   }// while
}// btrfs_parse_sys_chunks

static void btrfs_read_chunk_tree(uint64_t chunk_root) {
   if (chunk_root == 0) {
      return;
   }// if
   static uint8_t chunk_node[BTRFS_MAX_NODE_SIZE] __attribute__((aligned(8)));
   if (btrfs_read_node(chunk_root, chunk_node, btrfs_nodesize) != 0) {
      return;
   }// if
   struct btrfs_header *hdr = (struct btrfs_header *)chunk_node;
   if (hdr->bytenr != chunk_root || hdr->level >= BTRFS_MAX_LEVEL) {
      return;
   }// if
   if (hdr->level > 0) {
      uint32_t max_ptrs = (btrfs_nodesize - sizeof(struct btrfs_header)) / sizeof(struct btrfs_key_ptr);
      if (hdr->nritems > max_ptrs) {
         return;
      }// if
      struct btrfs_key_ptr *ptrs = (struct btrfs_key_ptr *)(chunk_node + sizeof(struct btrfs_header));
      uint32_t n = hdr->nritems;
      for (uint32_t i = 0; i < n && i < 16; i++) {
         uint64_t child_ptr = ptrs[i].blockptr;
         if (child_ptr == 0 || child_ptr == chunk_root) {
            continue;
         }// if
         static uint8_t child_node[BTRFS_MAX_NODE_SIZE] __attribute__((aligned(8)));
         if (btrfs_read_node(child_ptr, child_node, btrfs_nodesize) == 0) {
            struct btrfs_header *chdr = (struct btrfs_header *)child_node;
            if (chdr->bytenr != child_ptr || chdr->level >= hdr->level) {
               continue;
            }// if
            if (chdr->level == 0) {
               uint32_t max_items = (btrfs_nodesize - sizeof(struct btrfs_header)) / sizeof(struct btrfs_item);
               if (chdr->nritems > max_items) {
                  continue;
               }// if
               struct btrfs_item *citems = (struct btrfs_item *)(child_node + sizeof(struct btrfs_header));
               for (uint32_t j = 0; j < chdr->nritems; j++) {
                  if (citems[j].key.type == BTRFS_CHUNK_ITEM_KEY) {
                     if (citems[j].size >= sizeof(struct btrfs_chunk) &&
                         citems[j].offset <= btrfs_nodesize &&
                         citems[j].offset + citems[j].size <= btrfs_nodesize - sizeof(struct btrfs_header)) {
                        uint32_t off = sizeof(struct btrfs_header) + citems[j].offset;
                        const struct btrfs_chunk *c = (const struct btrfs_chunk *)(child_node + off);
                        if (c->num_stripes > 0) {
                           btrfs_add_chunk(citems[j].key.offset, c->length, c->stripe.offset);
                        }// if
                     }// if
                  }// if
               }// for
            }// if
         }// if
      }// for
   }// if
   else {
      uint32_t max_items = (btrfs_nodesize - sizeof(struct btrfs_header)) / sizeof(struct btrfs_item);
      if (hdr->nritems > max_items) {
         return;
      }// if
      struct btrfs_item *items = (struct btrfs_item *)(chunk_node + sizeof(struct btrfs_header));
      for (uint32_t i = 0; i < hdr->nritems; i++) {
         if (items[i].key.type == BTRFS_CHUNK_ITEM_KEY) {
            if (items[i].size >= sizeof(struct btrfs_chunk) &&
                items[i].offset <= btrfs_nodesize &&
                items[i].offset + items[i].size <= btrfs_nodesize - sizeof(struct btrfs_header)) {
               uint32_t off = sizeof(struct btrfs_header) + items[i].offset;
               const struct btrfs_chunk *c = (const struct btrfs_chunk *)(chunk_node + off);
               if (c->num_stripes > 0) {
                  btrfs_add_chunk(items[i].key.offset, c->length, c->stripe.offset);
               }// if
            }// if
         }// if
      }// for
   }// else
}// btrfs_read_chunk_tree

static int btrfs_read_leaf_for_key_from_root(uint64_t start_node, uint64_t obj, uint8_t type, uint64_t off, uint8_t *out_leaf) {
   if (!out_leaf || start_node == 0) {
      return -1;
   }// if
   uint64_t cur_node = start_node;
   uint8_t expected_level = BTRFS_MAX_LEVEL;
   uint64_t visited_nodes[BTRFS_MAX_LEVEL];
   uint32_t depth = 0;

   while (depth < BTRFS_MAX_LEVEL) {
      for (uint32_t v = 0; v < depth; v++) {
         if (visited_nodes[v] == cur_node) {
            errorn("btrfs: cyclic B-tree reference detected");
            return -1;
         }// if
      }// for
      visited_nodes[depth] = cur_node;
      depth++;

      if (btrfs_read_node(cur_node, out_leaf, btrfs_nodesize) != 0) {
         return -1;
      }// if
      struct btrfs_header *hdr = (struct btrfs_header *)out_leaf;
      if (hdr->bytenr != cur_node) {
         errorn("btrfs: header bytenr mismatch");
         return -1;
      }// if
      if (hdr->level >= expected_level) {
         errorn("btrfs: invalid or increasing tree level");
         return -1;
      }// if
      expected_level = hdr->level;
      if (hdr->level == 0) {
         uint32_t max_items = (btrfs_nodesize - sizeof(struct btrfs_header)) / sizeof(struct btrfs_item);
         if (hdr->nritems > max_items) {
            return -1;
         }// if
         return 0;
      }// if
      uint32_t max_ptrs = (btrfs_nodesize - sizeof(struct btrfs_header)) / sizeof(struct btrfs_key_ptr);
      if (hdr->nritems == 0 || hdr->nritems > max_ptrs) {
         return -1;
      }// if
      struct btrfs_key_ptr *ptrs = (struct btrfs_key_ptr *)(out_leaf + sizeof(struct btrfs_header));
      uint32_t target_idx = 0;
      for (uint32_t i = 0; i < hdr->nritems; i++) {
         if (btrfs_key_compare(&ptrs[i].key, obj, type, off) <= 0) {
            target_idx = i;
         }// if
         else {
            break;
         }// else
      }// for
      if (ptrs[target_idx].blockptr == 0) {
         return -1;
      }// if
      cur_node = ptrs[target_idx].blockptr;
   }// while
   errorn("btrfs: tree search depth limit exceeded");
   return -1;
}// btrfs_read_leaf_for_key_from_root

static int btrfs_find_leaf_item(uint64_t root_node, uint8_t level,
                                uint64_t obj, uint8_t type, uint64_t off,
                                void *out_buf, uint32_t *out_size) {
   (void)level;
   if (root_node == 0) {
      return -1;
   }// if
   static uint8_t leaf_buf[BTRFS_MAX_NODE_SIZE] __attribute__((aligned(8)));
   uint64_t search_off = (off == (uint64_t)-1) ? 0 : off;
   if (btrfs_read_leaf_for_key_from_root(root_node, obj, type, search_off, leaf_buf) != 0) {
      return -1;
   }// if
   struct btrfs_header *hdr = (struct btrfs_header *)leaf_buf;
   uint32_t max_items = (btrfs_nodesize - sizeof(struct btrfs_header)) / sizeof(struct btrfs_item);
   if (hdr->nritems > max_items) {
      return -1;
   }// if
   struct btrfs_item *items = (struct btrfs_item *)(leaf_buf + sizeof(struct btrfs_header));
   for (uint32_t i = 0; i < hdr->nritems; i++) {
      if (items[i].key.objectid == obj && items[i].key.type == type) {
         if (off == (uint64_t)-1 || items[i].key.offset == off) {
            if (items[i].offset <= btrfs_nodesize &&
                items[i].size <= btrfs_nodesize &&
                items[i].offset + items[i].size <= btrfs_nodesize - sizeof(struct btrfs_header)) {
               uint32_t data_offset = sizeof(struct btrfs_header) + items[i].offset;
               if (out_buf && out_size) {
                  uint32_t copy_len = items[i].size < *out_size ? items[i].size : *out_size;
                  memcpy(out_buf, leaf_buf + data_offset, copy_len);
                  *out_size = items[i].size;
               }// if
               return 0;
            }// if
         }// if
      }// if
   }// for
   return -1;
}// btrfs_find_leaf_item

static int btrfs_lookup_dir(uint64_t dir_ino, const char *name, uint64_t *out_ino, uint8_t *out_type) {
   if (!name || dir_ino == 0) {
      return -1;
   }// if
   static uint8_t leaf_buf[BTRFS_MAX_NODE_SIZE] __attribute__((aligned(8)));
   if (btrfs_read_leaf_for_key_from_root(btrfs_fs_tree_root, dir_ino, BTRFS_DIR_ITEM_KEY, 0, leaf_buf) != 0) {
      return -1;
   }// if
   struct btrfs_header *hdr = (struct btrfs_header *)leaf_buf;
   uint32_t max_items = (btrfs_nodesize - sizeof(struct btrfs_header)) / sizeof(struct btrfs_item);
   if (hdr->nritems > max_items) {
      return -1;
   }// if
   struct btrfs_item *items = (struct btrfs_item *)(leaf_buf + sizeof(struct btrfs_header));
   size_t target_name_len = strlen(name);
   if (target_name_len == 0 || target_name_len > 255) {
      return -1;
   }// if

   for (uint32_t i = 0; i < hdr->nritems; i++) {
      if (items[i].key.objectid == dir_ino &&
          (items[i].key.type == BTRFS_DIR_ITEM_KEY || items[i].key.type == BTRFS_DIR_INDEX_KEY)) {
         if (items[i].offset > btrfs_nodesize ||
             items[i].size > btrfs_nodesize ||
             items[i].size < sizeof(struct btrfs_dir_item) ||
             items[i].offset + items[i].size > btrfs_nodesize - sizeof(struct btrfs_header)) {
            continue;
         }// if
         uint32_t data_offset = sizeof(struct btrfs_header) + items[i].offset;
         uint32_t cur = 0;
         while (cur <= items[i].size - sizeof(struct btrfs_dir_item)) {
            struct btrfs_dir_item *di = (struct btrfs_dir_item *)(leaf_buf + data_offset + cur);
            uint32_t name_len = di->name_len;
            uint32_t data_len = di->data_len;
            if (name_len == 0 || name_len > 255) {
               break;
            }// if
            uint32_t total = sizeof(struct btrfs_dir_item) + name_len + data_len;
            if (total > items[i].size - cur) {
               break;
            }// if
            const char *item_name = (const char *)(leaf_buf + data_offset + cur + sizeof(struct btrfs_dir_item));
            if (name_len == target_name_len && memcmp(item_name, name, target_name_len) == 0) {
               if (out_ino) {
                  *out_ino = di->location.objectid;
               }// if
               if (out_type) {
                  *out_type = di->type;
               }// if
               return 0;
            }// if
            cur += total;
         }// while
      }// if
   }// for
   return -1;
}// btrfs_lookup_dir

static int btrfs_resolve_path(const char *path, uint64_t *out_ino) {
   if (!path) {
      return -1;
   }// if
   size_t path_len = 0;
   while (path[path_len] != '\0') {
      path_len++;
      if (path_len >= BTRFS_MAX_PATH_LEN) {
         return -1;
      }// if
   }// while
   if (path_len == 0 || path[0] != '/') {
      return -1;
   }// if

   uint64_t current_ino = BTRFS_FIRST_FREE_OBJECTID;
   uint64_t ino_stack[BTRFS_MAX_TRAVERSAL_DEPTH];
   uint32_t depth = 0;
   ino_stack[0] = BTRFS_FIRST_FREE_OBJECTID;

   const char *p = path;
   while (*p == '/') {
      p++;
   }// while

   if (*p == '\0') {
      if (out_ino) {
         *out_ino = current_ino;
      }// if
      return 0;
   }// if

   char segment[256];
   while (*p != '\0') {
      size_t len = 0;
      while (*p != '/' && *p != '\0') {
         if (len < sizeof(segment) - 1) {
            segment[len++] = *p;
         }// if
         else {
            return -1;
         }// else
         p++;
      }// while
      segment[len] = '\0';
      while (*p == '/') {
         p++;
      }// while

      if (len == 0 || (len == 1 && segment[0] == '.')) {
         continue;
      }// if

      if (len == 2 && segment[0] == '.' && segment[1] == '.') {
         if (depth > 0) {
            depth--;
            current_ino = ino_stack[depth];
         }// if
         else {
            current_ino = BTRFS_FIRST_FREE_OBJECTID;
         }// else
         continue;
      }// if

      if (depth + 1 >= BTRFS_MAX_TRAVERSAL_DEPTH) {
         return -1;
      }// if

      uint64_t next_ino = 0;
      if (btrfs_lookup_dir(current_ino, segment, &next_ino, NULL) != 0) {
         return -1;
      }// if
      depth++;
      ino_stack[depth] = next_ino;
      current_ino = next_ino;
   }// while

   if (out_ino) {
      *out_ino = current_ino;
   }// if
   return 0;
}// btrfs_resolve_path

int btrfs_lookup(const char *path, uint64_t *out_ino) {
   if (!btrfs_mounted || !path) {
      return -1;
   }// if
   return btrfs_resolve_path(path, out_ino);
}// btrfs_lookup

int btrfs_read(const char *path, void *buffer, uint32_t count, uint32_t offset) {
   if (!btrfs_mounted || !path || !buffer) {
      return -1;
   }// if
   if (count == 0) {
      return 0;
   }// if
   if (count > UINT32_MAX - offset) {
      count = UINT32_MAX - offset;
   }// if

   uint64_t ino = 0;
   if (btrfs_resolve_path(path, &ino) != 0) {
      return -1;
   }// if

   static uint8_t extent_buf[1024] __attribute__((aligned(8)));
   uint32_t extent_size = sizeof(extent_buf);
   if (btrfs_find_leaf_item(btrfs_fs_tree_root, btrfs_fs_tree_level,
                            ino, BTRFS_EXTENT_DATA_KEY, (uint64_t)-1,
                            extent_buf, &extent_size) != 0) {
      return -1;
   }// if

   if (extent_size < 21) {
      return -1;
   }// if
   struct btrfs_file_extent_item *fi = (struct btrfs_file_extent_item *)extent_buf;
   if (fi->type == BTRFS_FILE_EXTENT_INLINE) {
      uint32_t inline_avail = extent_size - 21;
      uint32_t inline_len = (uint32_t)fi->ram_bytes;
      if (inline_len > inline_avail) {
         inline_len = inline_avail;
      }// if
      if (offset >= inline_len) {
         return 0;
      }// if
      uint32_t available = inline_len - offset;
      uint32_t to_copy = count < available ? count : available;
      memcpy(buffer, fi->inline_data + offset, to_copy);
      return (int)to_copy;
   }// if
   else if (fi->type == BTRFS_FILE_EXTENT_REG) {
      if (extent_size < 53) {
         return -1;
      }// if
      if (offset >= fi->reg.num_bytes) {
         return 0;
      }// if
      uint64_t available_64 = fi->reg.num_bytes - offset;
      uint32_t available = available_64 > UINT32_MAX ? UINT32_MAX : (uint32_t)available_64;
      uint32_t to_read = count < available ? count : available;

      if (fi->reg.disk_bytenr == 0) {
         memset(buffer, 0, to_read);
         return (int)to_read;
      }// if

      static uint8_t sec_buf[512] __attribute__((aligned(8)));
      uint32_t bytes_copied = 0;

      while (bytes_copied < to_read) {
         uint64_t cur_offset = (uint64_t)offset + bytes_copied;
         if (fi->reg.disk_bytenr > UINT64_MAX - fi->reg.offset ||
             fi->reg.disk_bytenr + fi->reg.offset > UINT64_MAX - cur_offset) {
            return -1;
         }// if
         uint64_t target_logical = fi->reg.disk_bytenr + fi->reg.offset + cur_offset;
         uint64_t phys_sec = 0;
         if (btrfs_logical_to_sector(target_logical, &phys_sec) != 0) {
            return -1;
         }// if

         uint32_t sec_offset = (uint32_t)(target_logical % 512);
         if (virtio_read_sector(phys_sec, sec_buf) != 0) {
            return -1;
         }// if
         uint32_t available_in_sec = 512 - sec_offset;
         uint32_t chunk = (to_read - bytes_copied < available_in_sec) ? (to_read - bytes_copied) : available_in_sec;
         memcpy((uint8_t *)buffer + bytes_copied, sec_buf + sec_offset, chunk);
         bytes_copied += chunk;
      }// while
      return (int)bytes_copied;
   }// else if

   return -1;
}// btrfs_read

int btrfs_list_dir(const char *path) {
   if (!btrfs_mounted || !path) {
      return -1;
   }// if

   uint64_t dir_ino = 0;
   if (btrfs_resolve_path(path, &dir_ino) != 0) {
      return -1;
   }// if

   static uint8_t leaf_buf[BTRFS_MAX_NODE_SIZE] __attribute__((aligned(8)));
   if (btrfs_read_leaf_for_key_from_root(btrfs_fs_tree_root, dir_ino, BTRFS_DIR_INDEX_KEY, 0, leaf_buf) != 0 &&
       btrfs_read_leaf_for_key_from_root(btrfs_fs_tree_root, dir_ino, BTRFS_DIR_ITEM_KEY, 0, leaf_buf) != 0) {
      return -1;
   }// if

   struct btrfs_header *hdr = (struct btrfs_header *)leaf_buf;
   uint32_t max_items = (btrfs_nodesize - sizeof(struct btrfs_header)) / sizeof(struct btrfs_item);
   if (hdr->nritems > max_items) {
      return -1;
   }// if
   struct btrfs_item *items = (struct btrfs_item *)(leaf_buf + sizeof(struct btrfs_header));

   infon("btrfs: directory contents:");
   uint32_t count = 0;
   for (uint32_t i = 0; i < hdr->nritems; i++) {
      if (items[i].key.objectid == dir_ino && items[i].key.type == BTRFS_DIR_INDEX_KEY) {
         if (items[i].offset > btrfs_nodesize ||
             items[i].size > btrfs_nodesize ||
             items[i].size < sizeof(struct btrfs_dir_item) ||
             items[i].offset + items[i].size > btrfs_nodesize - sizeof(struct btrfs_header)) {
            continue;
         }// if
         uint32_t data_offset = sizeof(struct btrfs_header) + items[i].offset;
         uint32_t cur = 0;
         while (cur <= items[i].size - sizeof(struct btrfs_dir_item)) {
            struct btrfs_dir_item *di = (struct btrfs_dir_item *)(leaf_buf + data_offset + cur);
            uint32_t name_len = di->name_len;
            uint32_t data_len = di->data_len;
            if (name_len == 0 || name_len > 255) {
               break;
            }// if
            uint32_t total = sizeof(struct btrfs_dir_item) + name_len + data_len;
            if (total > items[i].size - cur) {
               break;
            }// if
            const char *name = (const char *)(leaf_buf + data_offset + cur + sizeof(struct btrfs_dir_item));
            char name_buf[256];
            size_t nlen = name_len < sizeof(name_buf) - 1 ? name_len : sizeof(name_buf) - 1;
            memcpy(name_buf, name, nlen);
            name_buf[nlen] = '\0';
            print_character("  ");
            print_character(name_buf);
            print_character("\n");
            cur += total;
            count++;
         }// while
      }// if
   }// for
   if (count == 0) {
      for (uint32_t i = 0; i < hdr->nritems; i++) {
         if (items[i].key.objectid == dir_ino && items[i].key.type == BTRFS_DIR_ITEM_KEY) {
            if (items[i].offset > btrfs_nodesize ||
                items[i].size > btrfs_nodesize ||
                items[i].size < sizeof(struct btrfs_dir_item) ||
                items[i].offset + items[i].size > btrfs_nodesize - sizeof(struct btrfs_header)) {
               continue;
            }// if
            uint32_t data_offset = sizeof(struct btrfs_header) + items[i].offset;
            uint32_t cur = 0;
            while (cur <= items[i].size - sizeof(struct btrfs_dir_item)) {
               struct btrfs_dir_item *di = (struct btrfs_dir_item *)(leaf_buf + data_offset + cur);
               uint32_t name_len = di->name_len;
               uint32_t data_len = di->data_len;
               if (name_len == 0 || name_len > 255) {
                  break;
               }// if
               uint32_t total = sizeof(struct btrfs_dir_item) + name_len + data_len;
               if (total > items[i].size - cur) {
                  break;
               }// if
               const char *name = (const char *)(leaf_buf + data_offset + cur + sizeof(struct btrfs_dir_item));
               char name_buf[256];
               size_t nlen = name_len < sizeof(name_buf) - 1 ? name_len : sizeof(name_buf) - 1;
               memcpy(name_buf, name, nlen);
               name_buf[nlen] = '\0';
               print_character("  ");
               print_character(name_buf);
               print_character("\n");
               cur += total;
            }// while
         }// if
      }// for
   }// if
   return 0;
}// btrfs_list_dir

void init_btrfs(void) {
   static uint8_t sb_buf[4096] __attribute__((aligned(8)));
   infon("btrfs: probing superblock at offset 64KB");

   btrfs_mounted = false;
   num_sys_chunks = 0;
   btrfs_fs_tree_root = 0;
   btrfs_fs_tree_level = 0;

   for (int i = 0; i < 8; i++) {
      if (virtio_read_sector(128 + i, sb_buf + (i * 512)) != 0) {
         errorn("btrfs: failed reading primary superblock sectors");
         return;
      }// if
   }// for

   struct btrfs_super_block *sb = (struct btrfs_super_block *)sb_buf;
   if (sb->magic != BTRFS_MAGIC) {
      errorn("btrfs: invalid magic signature");
      return;
   }// if

   if (sb->sectorsize < 512 || sb->sectorsize > 65536 || (sb->sectorsize & (sb->sectorsize - 1)) != 0) {
      btrfs_sectorsize = 4096;
   }// if
   else {
      btrfs_sectorsize = sb->sectorsize;
   }// else

   if (sb->nodesize < sizeof(struct btrfs_header) || sb->nodesize > BTRFS_MAX_NODE_SIZE || (sb->nodesize & (sb->nodesize - 1)) != 0) {
      errorn("btrfs: invalid node size in superblock");
      return;
   }// if
   else {
      btrfs_nodesize = sb->nodesize;
   }// else

   if (sb->root == 0 || sb->chunk_root == 0) {
      errorn("btrfs: null root or chunk root pointer");
      return;
   }// if

   if (sb->root_level >= BTRFS_MAX_LEVEL || sb->chunk_root_level >= BTRFS_MAX_LEVEL) {
      errorn("btrfs: tree level exceeds maximum allowed depth");
      return;
   }// if

   btrfs_parse_sys_chunks(sb);
   if (num_sys_chunks == 0) {
      errorn("btrfs: failed to parse bootstrap system chunks");
      return;
   }// if

   btrfs_read_chunk_tree(sb->chunk_root);

   btrfs_fs_tree_root = sb->root;
   btrfs_fs_tree_level = sb->root_level;

   struct btrfs_root_item ri;
   uint32_t ri_size = sizeof(ri);
   if (btrfs_find_leaf_item(sb->root, sb->root_level,
                            BTRFS_FS_TREE_OBJECTID, BTRFS_ROOT_ITEM_KEY, (uint64_t)-1,
                            &ri, &ri_size) == 0 && ri_size >= sizeof(ri)) {
      if (ri.bytenr != 0 && ri.level < BTRFS_MAX_LEVEL) {
         btrfs_fs_tree_root = ri.bytenr;
         btrfs_fs_tree_level = ri.level;
      }// if
   }// if

   btrfs_mounted = true;
   successn("btrfs: filesystem successfully mounted and ready");
}// init_btrfs

bool btrfs_is_mounted(void) {
   return btrfs_mounted;
}// btrfs_is_mounted
