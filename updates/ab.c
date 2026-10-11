#include "../include/ab_update.h"
#include "../include/kernel.h"
#include "../include/userspace.h"

#ifdef __USE_AB__

uint32_t install_update(char use_slot, const char *update_file) {
   static uint8_t base_buf[1024 * 512];
   static uint8_t patch_buf[1024 * 64];

   int base_sz = read("/boot/kernel", base_buf, sizeof(base_buf), 0);
   int patch_sz = read(update_file, patch_buf, sizeof(patch_buf), 0);
   if (base_sz <= 0 || patch_sz <= 0) {
      errorn("Failed to read base kernel or patch");
      return 1;
   }// if

   int new_sz = apply_patch(base_buf, (uint32_t)base_sz, patch_buf, (uint32_t)patch_sz);
   if (new_sz <= 0) {
      errorn("Patch application failed");
      return 1;
   }// if

   const char *slot_path = (use_slot == 'a') ? "/boot/a/kernel.bin" : "/boot/b/kernel.bin";
   write(slot_path, base_buf, (uint32_t)new_sz, 0);
   write("/boot/kernel", base_buf, (uint32_t)new_sz, 0);

   successn("Update applied successfully, rebooting");
   flip_active_slot();
   reboot();
   return 0;
}// install_update

void update_protection(void) {
   if (get_slot_status()) {
      successn("Slot state perfect, starting"); 
   } else {
      errorn("Failed to boot, flipping slot");
      flip_active_slot();
      reboot();
   }// if
}// update_protection

#endif

