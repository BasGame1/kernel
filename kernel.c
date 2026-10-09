#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "include/kernel.h"
#include "include/shell.h"

extern void kernel_init() {
   /* screen init */
   start_screen();
   successn("Kernel Starting\n");
   
   /* memory init */
   infon("Starting MMU\n");
   init_memory();
   successn("Memory Started\n");
   
   /* virtual memory init */
   infon("Starting Virtual Memory\n");
   init_virtual_memory();
   successn("Virtual Memory Started\n");
   
   /* trap init */
   infon("Starting Trap Handlers\n");
   for(uint32_t i = 0; i < 16; i++) {
      interrupt_handlers[i] = unknown_error;
      exception_handlers[i] = unknown_error;
   }// for jump table
   
   exception_handlers[2] = handle_illegal_instruction;
   exception_handlers[3] = handle_breakpoint;
   exception_handlers[5] = handle_load_access_error;
   exception_handlers[7] = handle_store_access_error;
   interrupt_handlers[7] = handle_clock_error;
   exception_handlers[12] = handle_load_page_error;
   exception_handlers[13] = handle_store_page_error;
   exception_handlers[15] = handle_instruction_page_error;
   asm volatile("csrw mtvec, %0" :: "r"(trap_vector));
   successn("Trap Handlers Started\n");
   
   /* clock init */
   infon("Starting Clock\n");
   init_mtimecmp();
   successn("Clock Started\n");
   
   /* test keyboard */
   
   #ifdef __DEBUG__
   debugn("Testing keyboard");
   if (test_keyboard()) {
      infon("Keyboard correct");
   } else {
      errorn("Keyboard error");
   }// if to see result
   #endif
   
   /* filesystem init */
   init_fs();
   init_ext2();
   
   /* shell init */
   #ifdef __NOSHELL__
      // Compilation without shell
   #else
      init_shell();
   #endif
}// kernel_init

extern void trap_handler() {
   uintptr_t cause;
   asm volatile("csrr %0, mcause" : "=r" (cause));
   bool is_hardware = (cause & 0x80000000) != 0;
   uint32_t trap_reason = cause & 0x7FFFFFFF;

   if (trap_reason > 15) {
      fataln("trap reason is bigger than 15");
      return;
   }
   
   if (is_hardware) {
      interrupt_handlers[trap_reason]();
   } else {
      exception_handlers[trap_reason]();
   }
}// trap_handler
