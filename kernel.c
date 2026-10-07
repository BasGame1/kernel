#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <bool.h>

#include "include/kernel.h"

#include "screen/status.c"
#include "screen/screen.c"
#include "utils/concat.c"
#include "clock/clock.c"
#include "utils/error.c"

extern void kernel_init() {

   /* trap init */
   for(int i = 0; i < 16; i++) {
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
   
   /* clock init */
   init_mtimecmp();
   
   /* screen init */
   start_screen();
   print("Kernel Starting");
  //TODO, add calling processes and tbh, everything else
}// kernel_init

extern void trap_handler() {
   asm volatile("csrr %0, mcause" : "=r" (cause));
   bool is_hardware = cause & ((1<<8)-1);
   int trap_reason =  cause & ~(1 << 0);

   if (trap_reason > 15) {
      fatal("trap reason is bigger than 15");
   }// if trap_reason gigger than 15
   
   if (is_hardware) {
      interrupt_handlers(trap_reason);
   } else {
      exception_handlers(trap_reason);
   }// if is_hardware
}// trap_handler
