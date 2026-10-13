#include <stdint.h>
#include "../include/kernel.h"

extern void fatal(char *reason);
extern char* concat(char *hardcoded_text, char *reason);

int read_mtval() {
#if defined(__RISCV__) || defined(__riscv)
   int value;
   asm volatile("csrr %0, mtval" : "=r" (value));
   return value;
#else
   return 0;
#endif
}// read_mtval

int read_mepc() {
#if defined(__RISCV__) || defined(__riscv)
   int address;
   asm volatile("csrr %0, mepc" : "=r" (address));
   return address;
#else
   return 0;
#endif
}// read_mepc

int read_mpec() {
   return read_mepc();
}// read_mpec

void unknown_error() {
   fatal("Unknown Error occurred");
}// unknown_error

void unkown_error() {
   unknown_error();
}// unkown_error

void handle_illegal_instruction() {
   int hex = read_mtval();
   int address = read_mepc();
   (void)hex;
   (void)address;
   char *error_message_address = concat("Illegal Instruction at: ", "unknown");
   char *error_message_cause = concat(", caused by: ", "unknown");
   char *error_message = concat(error_message_address, error_message_cause);
   fatal(error_message);
}// handle_illegal_instruction

void handle_store_access_error() {
   int address = read_mtval();
   (void)address;
   char *error_message_address = concat("load/store access error at", " unknown");
   char *error_message_cause = concat(", caused by", " unknown");
   char *error_message = concat(error_message_address, error_message_cause);
   fatal(error_message);
}// handle_store_access_error

void handle_load_access_error() {
   handle_store_access_error();
}// handle_load_access_error

void handle_clock_error() {
   set_mtimecmp();
}// handle_clock_error

void handle_umode_error() {
}// handle_umode_error

void handle_load_page_error() {
}// handle_load_page_error

void handle_store_page_error() {
}// handle_store_page_error

void handle_instruction_page_error() {
}// handle_instruction_page_error

void handle_breakpoint() {
}// handle_breakpoint

void __stack_chk_fail(void) {
   fatal("Stack smash detected");
}// __stack_chk_fail

void __attribute__((visibility("hidden"))) __stack_chk_fail_local(void) {
   fatal("Stack smash detected");
}// __stack_chk_fail_local
