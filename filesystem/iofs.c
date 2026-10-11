#include <stdint.h>

#include "../include/kernel.h"
#include "../include/userspace.h"

/* Use ext2 as default filesystem */

int read(const char *path, void *buffer, uint32_t count, uint32_t offset) {
   return ext2_read(path, buffer, count, offset);
}// read

int write(const char *path, const void *buffer, uint32_t count, uint32_t offset) {
   return ext2_write(path, buffer, count, offset);
}// write

int update(const char *path, uint32_t mode, uint32_t mtime) {
   return ext2_update(path, mode, mtime);
}// update

int remove(const char *path) {
   return ext2_remove(path);
}// remove

int kmkdir(const char *path) {
   return ext2_mkdir(path);
}// mkdir

void kcp(char *origin, char *destiny) {
   char *buffer[4096];
   read(origin, buffer, 4096, 0);
   write(destiny, buffer, 4096, 0);
}// cp

void kmv(char *origin, char *destiny) {
   // horrible mv implementation, ik
   char *buffer[4096];
   read(origin, buffer, 4096, 0);
   write(destiny, buffer, 4096, 0);
   remove(origin);
}// mv

void ktouch(char *path) {
   ext2_create_file(path);
}// path

