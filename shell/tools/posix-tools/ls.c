#include <stdbool.h>
#include <stdint.h>

#include "../../../include/shell.h"
#include "../../../include/kernel.h"

void ls(char *path) {
   if (path == NULL || *path == '\0') {
      printn("Indicate a path");
   } else {
      ext2_list_dir(path);
   }
}
