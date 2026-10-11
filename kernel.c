#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "include/kernel.h"
#include "include/shell.h"
#include "include/userspace.h"

extern void kernel_init() {
   /* screen init */
   start_screen();
   successn("Kernel Starting");
   
   #ifdef __USE_AB__
      /* Update protection */
      update_protection();
   #endif
   
   /* memory init */
   infon("Starting MMU");
   init_memory();
   successn("Memory Started");
   
   /* virtual memory init */
   infon("Starting Virtual Memory");
   init_virtual_memory();
   successn("Virtual Memory Started");
   
   /* trap init */
   infon("Starting Trap Handlers");
   for(uint32_t i = 0; i < 16; i++) {
      interrupt_handlers[i] = unknown_error;
      exception_handlers[i] = unknown_error;
   }// for jump table
   
   exception_handlers[2] = handle_illegal_instruction;
   exception_handlers[3] = handle_breakpoint;
   exception_handlers[5] = handle_load_access_error;
   exception_handlers[7] = handle_store_access_error;
   interrupt_handlers[7] = handle_clock_error;
   exception_handlers[8] = test_call; // test kernel calls
   exception_handlers[12] = handle_instruction_page_error;
   exception_handlers[13] = handle_load_page_error;
   exception_handlers[15] = handle_store_page_error;
   asm volatile("csrw mtvec, %0" :: "r"(trap_vector));
   successn("Trap Handlers Started");
   
   /* clock init */
   infon("Starting Clock");
   init_mtimecmp();
   successn("Clock Started");
   
   /* test keyboard */
   
   #ifdef __DEBUG__
   debugn("Testing keyboard");
   if (test_keyboard()) {
      infon("Keyboard correct");
   } else {
      errorn("Keyboard error");
   }// if to see result
   #endif
   
  /*
   * // filesystem init
   *infon("Starting filesystem");
   *init_fs();
   *infon("Starting ext2 fs");
   *init_ext2();
   */
   
   /* 
    * shell init
    * This kernel-shell abomination should only be called on debug version 
    * to diagnose errors at boot
    */
    
   #ifdef __DEBUG__
      #ifdef __NOSHELL__
         debugn("Not opening shell due to noshell compilation");
      #else
         infon("Starting shell");
         init_shell();
      #endif
   #endif
   
   /* print init */
   create_print_process();

   /* screen init */
   create_screen_process();
   
   /* keyboard init */
   create_keyboard_process();
   
   /* virtio init */
   create_virtio_process();
   
   /* fs init */
   #ifdef __USE_EXT2__
      create_ext2_process();
   #endif
   
   #ifndef __USE_NTAR__
      /* Dir creation */
      create_dirs();
      create_slot_dirs();
   #else
     //TODO, add ntar
   #endif
   
   #if defined(__USE_AB__) || defined(__USE_AB_NO_REBOOT__)
      /* updates init */
      create_update_process();
   #endif
   
   /* userspace init */
   infon("Starting userspace");
   userspace_init();
   
}// kernel_init

extern struct trap_frame *trap_handler(struct trap_frame *frame) {
   uintptr_t cause;
   asm volatile("csrr %0, mcause" : "=r" (cause));
   bool is_hardware = (cause & 0x80000000) != 0;
   uint32_t trap_reason = cause & 0x7FFFFFFF;

   if (trap_reason > 15) {
      fataln("trap reason is bigger than 15");
      return frame;
   }
   
   if (is_hardware) {
      interrupt_handlers[trap_reason]();
   } else {
      if (trap_reason == 8) {
         kcall_analyze(frame);
         return schedule(frame);
      } else {
         exception_handlers[trap_reason]();
      }// if trap reason
   }// if hardware
   return schedule(frame);
}// trap_handler
