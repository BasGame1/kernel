#include "../include/noab_update.h"

#if !defined(__USE_AB__) && !defined(__USE_AB_NO_REBOOT__)

uint32_t install_update(char use_slot, const char *file_path) {
   (void)use_slot;
   (void)file_path;
   return 0;
}// install_update

#endif

