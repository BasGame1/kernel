#include "../include/abnr_update.h"
#include "../include/kernel.h"
#include "../include/userspace.h"

#ifdef __USE_AB_NO_REBOOT__

void make_security_copy(void) {
   kmkdir("/boot/seccopy");
   kcp("/boot/kernel", "/boot/seccopy/kernel.bak");
   successn("Security copy made");
}// make_security_copy

uint32_t revert_changes(void) {
   remove("/boot/kernel");
   uint32_t status = (kcp("/boot/seccopy/kernel.bak", "/boot/kernel") == 0) ? 0 : 1;
   return status;
}// revert_changes

uint32_t install_update(char active_slot, const char *update_file) {
   (void)active_slot;
   make_security_copy();
   kmkdir("/boot/update");
   kcp("/boot/kernel", "/boot/update/live-kernel");

   static uint8_t base_buf[1024 * 512];
   static uint8_t patch_buf[1024 * 64];

   int base_sz = read("/boot/kernel", base_buf, sizeof(base_buf), 0);
   int patch_sz = read(update_file, patch_buf, sizeof(patch_buf), 0);
   if (base_sz <= 0 || patch_sz <= 0) {
      errorn("Failed to read base kernel or patch");
      return 1;
   }// if

   int new_sz = apply_patch(base_buf, (uint32_t)base_sz, patch_buf, (uint32_t)patch_sz);
   if (new_sz > 0) {
      write("/boot/kernel", base_buf, (uint32_t)new_sz, 0);
      memcpy((void *)0x80200000, base_buf, (uint32_t)new_sz);
      successn("Kernel patched into 0x80200000, handing off CPU");
      // Kernel handoff
      asm volatile("csrci mstatus, 8");
      asm volatile("csrw mie, zero");
      asm volatile("fence.i");
      asm volatile("fence");
      asm volatile("jr %0" :: "r"(0x80200000));
      return 0;
   } else {
      errorn("Update failed, DO NOT REBOOT");
      if (revert_changes() == 0) {
         successn("Changes reverted, everything is fine now, just breathe");
         return 2;
      } else {
         errorn("Changes could not be reverted, say welcome to troubleshoot");
         return 3;
      }// if
   }// if
}// install_update

#endif

