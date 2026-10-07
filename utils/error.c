#include <stdint.h>
#include "../include/kernel.h"

extern void fatal(char *reason);
extern char* concat(char *hardcoded_text, char *reason);

// utils for getting the register
int read_mtval() {
   int value;
   asm volatile("csrr %0, mtval" : "=r" (value));
   return value;
}// read_mtval

int read_mpec() {
   int address;
   asm volatile("csrr %0, mepc" : "=r" (address));
   return address;
}// read_mpec

/* Functions of all the handlers for errors
 * Userspace is still not implemented, so there 
 * are only declarations of the functions
 */ 

/* unkown error */

void unknown_error() {
   fatal("Unknown Error occurred");
}// unknown_error

void unkown_error() {
   unknown_error();
}// unkown_error


/* hardware (interrupts) */

// On header file, only declared //


/* Software (exceptions) */
 
// Rest on header file, only declared //

void handle_illegal_instruction() {
   int hex = read_mtval();
   int address = read_mpec();
   (void)hex;
   (void)address;
   char *error_message_address = concat("Illegal Instruction at: ", "unknown");
   char *error_message_cause = concat(", caused by: ", "unknown");
   char *error_message = concat(error_message_address, error_message_cause);
   fatal(error_message);
}// handle__illegal_instructions

void handle_store_access_error() {
  int address = read_mtval();
  (void)address;
  char *error_message_address = concat("load/store access error at", " unknown");
  char *error_message_cause = concat(", caused by", " unknown");
  char *error_message = concat(error_message_address, error_message_cause);
  fatal(error_message);
}// handle_store_access_error & handle_load_access_error [via macro]

void handle_load_access_error() {
   handle_store_access_error();
}// handle_load_access_error

void handle_clock_error() {}// handle_clock_error
void handle_umode_error() {}// handle_umode_error
void handle_load_page_error() {}// handle_load_page_error
void handle_store_page_error() {}// handle_store_page_error
void handle_instruction_page_error() {}// handle_instruction_page_error
void handle_breakpoint() {}// handle_breakpoint

