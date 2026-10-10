#include <stdint.h>

#include "../include/userspace.h"

void create_dirs() {
   uint32_t i = 0;
   char *buffer[4096];
   char *dirs[] = { "/root", "/hw", "/boot", "/bin", "/lib", "/tmp", "/other", "/user" };
   while (i < sizeof(dirs)) {
      write(dirs[i], *buffer, 0, 0);
   }// while
}// create_dirs
