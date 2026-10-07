#include "../include/kernel.h"
#include "../include/error.h"

#include "../screen/status.c"
#include "concat.c"

// utils for getting the register
int read_mtval() {
   asm volatile("csrr %0, mtval" : "=r" (value));
   return value;
}// read_mtval

int read_mpec() {
   asm volatile("csrr %0, mpec" : "=r" (address));
   return address;
}// read_mpec

/* Functions of all the handlers for errors
 * Userspace is still not implemented, so there 
 * are only declarations of the functions
 */ 

/* unkown error */

void unkown_error() {
   fatal("Unkown Error occurred");
}// unkown_error


/* hardware (interrupts) */

// On header file, only declared //


/* Software (exceptions) */
 
// Rest on header file, only declared //

void handle_illegal_instruction() {
   int hex = read_mtval();
   int address = read_mpec();
   error_message_address = concat("Illegal Instruction at: ", address);
   error_message_cause = concat(", caused by: ", hex);
   error_message = concat(error_message_address, error_message_cause);
   fatal(error_message);
}// handle__illegal_instructions

void handle_store_access_error() {
  int address = read_mtval();
  error_message_address = concat("load/store access error at", address);
  error_message_cause = concat(", caused by", cause);
  fatal(error_message);
}// handle_store_access_error & handle_load_access_error [via macro]

