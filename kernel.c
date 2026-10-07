#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "include/kernel.h"
#include "include/screen.h"

extern void init_memory(void);
extern void init_virtual_memory(void);
extern void init_mtimecmp(void);
extern void start_screen(void);
extern void print_character(char *message);
extern void unknown_error(void);
extern void handle_illegal_instruction(void);
extern void handle_breakpoint(void);
extern void handle_load_access_error(void);
extern void handle_store_access_error(void);
extern void handle_clock_error(void);
extern void handle_load_page_error(void);
extern void handle_store_page_error(void);
extern void handle_instruction_page_error(void);
extern void fatal(char *reason);

extern void kernel_init() {
   /* memory init */
   init_memory();
   
   /* virtual memory init */
   init_virtual_memory();
   
   /* trap init */
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
   
   /* clock init */
   init_mtimecmp();
   
   /* screen init */
   start_screen();
   print("Kernel Starting");
  //TODO, add calling processes and tbh, everything else
}// kernel_init

extern void trap_handler() {
   uintptr_t cause;
   asm volatile("csrr %0, mcause" : "=r" (cause));
   bool is_hardware = cause & ((1<<8)-1);
   uint32_t trap_reason =  cause & ~(1 << 0);

   if (trap_reason > 15) {
      fatal("trap reason is bigger than 15");
   }// if trap_reason gigger than 15
   
   if (is_hardware) {
      interrupt_handlers[trap_reason]();
   } else {
      exception_handlers[trap_reason]();
   }// if is_hardware
}// trap_handler
