#include <stdint.h>

#include "../include/kernel.h"
#include "../include/userspace.h"

/* Use ext2 as default filesystem */

int read(const char *path, void *buffer, uint32_t count, uint32_t offset) {
   return ext2_read(path, buffer, count, offset);
}

int write(const char *path, const void *buffer, uint32_t count, uint32_t offset) {
   return ext2_write(path, buffer, count, offset);
}

int update(const char *path, uint32_t mode, uint32_t mtime) {
   return ext2_update(path, mode, mtime);
}

int remove(const char *path) {
   return ext2_remove(path);
}
