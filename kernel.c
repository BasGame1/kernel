#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "include/kernel.h"
#include "include/shell.h"
#include "include/userspace.h"

uintptr_t mtime = 0;
uintptr_t mtimemcp = 0;
handler_t interrupt_handlers[16];
handler_t exception_handlers[16];

extern void kernel_init() {
   start_screen();
   successn("Kernel Starting");
   
   #ifdef __USE_AB__
      update_protection();
   #endif
   
   infon("Starting MMU");
   init_memory();
   successn("Memory Started");
   
   infon("Starting Virtual Memory");
   init_virtual_memory();
   successn("Virtual Memory Started");
   
   infon("Starting Trap Handlers");
   for (uint32_t i = 0; i < 16; i++) {
      interrupt_handlers[i] = unknown_error;
      exception_handlers[i] = unknown_error;
   }// for jump table
   
   exception_handlers[2] = handle_illegal_instruction;
   exception_handlers[3] = handle_breakpoint;
   exception_handlers[5] = handle_load_access_error;
   exception_handlers[7] = handle_store_access_error;
   interrupt_handlers[7] = handle_clock_error;
   exception_handlers[8] = test_call;
   exception_handlers[12] = handle_instruction_page_error;
   exception_handlers[13] = handle_load_page_error;
   exception_handlers[15] = handle_store_page_error;
#if defined(__RISCV__) || defined(__riscv)
   asm volatile("csrw mtvec, %0" :: "r"(trap_vector));
#endif
   successn("Trap Handlers Started");
   
   infon("Starting Clock");
   init_mtimecmp();
   successn("Clock Started");
   
   #ifdef __DEBUG__
   debugn("Testing keyboard");
   if (test_keyboard()) {
      infon("Keyboard correct");
   } else {
      errorn("Keyboard error");
   }// if to see result
   #endif
   
   #ifdef __DEBUG__
      #ifdef __NOSHELL__
         debugn("Not opening shell due to noshell compilation");
      #else
         infon("Starting shell");
         init_shell();
      #endif
   #endif
   
   create_print_process();

   init_modules();

   #ifdef __LOAD_SCREEN_AS_MODULE__
      register_screen_module();
   #else
      create_screen_process();
   #endif
   
   #ifdef __LOAD_KEYBOARD_AS_MODULE__
      register_keyboard_module();
   #else
      create_keyboard_process();
   #endif
   
   #ifdef __LOAD_VIRTIO_AS_MODULE__
      register_virtio_module();
   #else
      create_virtio_process();
   #endif
   
   #ifdef __LOAD_DEFAULT_FS_AS_MODULE__
      register_default_fs_module();
   #else
      #ifdef __USE_EXT2__
         create_ext2_process();
      #endif
   #endif
   
   #ifndef __USE_NTAR__
      create_dirs();
      create_slot_dirs();
   #else
      ntar_init();
   #endif
   
   #if defined(__USE_AB__) || defined(__USE_AB_NO_REBOOT__)
      create_update_process();
   #endif

   #ifdef __USE_ANTARTICA__
      create_antartica_process();
   #endif
   
   infon("Starting userspace");
   userspace_init();
   
}// kernel_init

extern struct trap_frame *trap_handler(struct trap_frame *frame) {
   if (!frame) {
      fataln("trap_handler received NULL frame");
      return frame;
   }// if

#if defined(__RISCV__) || defined(__riscv)
   uintptr_t cause;
   asm volatile("csrr %0, mcause" : "=r" (cause));
   bool is_hardware = ((intptr_t)cause < 0);
   uint32_t trap_reason = (uint32_t)(cause & 0xFFF);
#else
   bool is_hardware = false;
   uint32_t trap_reason = 8;
#endif

   if (trap_reason > 15) {
      fataln("trap reason is bigger than 15");
      return schedule(frame);
   }// if
   
   if (is_hardware) {
      if (interrupt_handlers[trap_reason]) {
         interrupt_handlers[trap_reason]();
      } else {
         unknown_error();
      }// if
   } else {
      if (trap_reason == 8 || trap_reason == 9 || trap_reason == 11) {
         kcall_analyze(frame);
         return schedule(frame);
      } else {
         if (exception_handlers[trap_reason]) {
            exception_handlers[trap_reason]();
         } else {
            unknown_error();
         }// if
      }// if trap reason
   }// if hardware
   return schedule(frame);
}// trap_handler
