#include <stdint.h>
#include <stddef.h>

#include "../include/kernel.h"

void create_dirs() {
   const char *dirs[] = { "/root", "/hw", "/boot", "/bin", "/lib", "/tmp", "/other", "/user", "/slot" };
   size_t count = sizeof(dirs) / sizeof(dirs[0]);
   for (size_t i = 0; i < count; i++) {
      kmkdir(dirs[i]);
   }// for
}// create_dirs

void create_slot_dirs() {
   kmkdir("/slot/a");
   kmkdir("/slot/b");
}// create_slot_dirs
