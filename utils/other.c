#include <stdint.h>
#include <stdbool.h>

#include "../include/hardware.h"
#include "../include/userspace.h"

bool dir_entry_matches(const struct ext2_dir_entry *entry, const char *name, uint32_t name_len) {
   if (entry == NULL || name == NULL) return false;
   if (entry->inode == 0 || entry->name_len != name_len) return false;
   for (uint32_t i = 0; i < name_len; i++) {
      if (entry->name[i] != name[i]) return false;
   }
   return true;
}
