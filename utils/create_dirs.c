#include <stdint.h>

#include "../include/kernel.h"

void create_dirs() {
   uint32_t i = 0;
   char *dirs[] = { "/root", "/hw", "/boot", "/bin", "/lib", "/tmp", "/other", "/user", "/slot" };
   while (i < sizeof(dirs)) {
      kmkdir(dirs[i]);
   }// while
}// create_dirs

void create_slot_dirs() {
   kmkdir("/slot/a");
   kmkdir("/slot/b");
}// create_slot_dirs
