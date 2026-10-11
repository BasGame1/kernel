#include "../include/userspace.h"
#include "../include/gid.h"

// Status functions

void debugn(char *reason) {
   char *message = concat("\e[0;93m[DEBUG]\e[0;0m ", reason);
   print(message);
   print("\n");
}// debug

void errorn(char *reason) {
   char *message = concat("\e[0;31m[ERROR]\e[0;0m ", reason);
   print(message);
   print("\n");
}// error

void fataln(char *reason) {
   char *message = concat("\e[0;91m[ERROR]\e[0;0m ", reason);
   print(message);
   print("\n");
   // TODO, add exit function 
}// fatal

void successn(char *reason) {
   char *message = concat("\e[0;32m[SUCCESS]\e[0;0m ", reason);
   print(message);
   print("\n");
}// success

void infon(char *reason) {
   char *message = concat("\e[0;33m[INFO]\e[0;0m ", reason);
   print(message);
   print("\n");
}// info


// Status functions without newline

void debug(char *reason) {
   char *message = concat("\e[0;93m[DEBUG]\e[0;0m ", reason);
   print(message);
}// debug

void error(char *reason) {
   char *message = concat("\e[0;31m[ERROR]\e[0;0m ", reason);
   print(message);
}// error

void fatal(char *reason) {
   char *message = concat("\e[0;91m[ERROR]\e[0;0m ", reason);
   print(message);
   // TODO, add exit function 
}// fatal

void success(char *reason) {
   char *message = concat("\e[0;32m[SUCCESS]\e[0;0m ", reason);
   print(message);
}// success

void info(char *reason) {
   char *message = concat("\e[0;33m[INFO]\e[0;0m ", reason);
   print(message);
}// info


// Print wrapper with newline char

void printn(char *message) {
   char *temporal = concat(message, "\n");
   print_character(temporal);
}// printn
    
void print_service_process(void) {
   while (1) {
      char msg_buf[64];
      uint32_t sender = user_ipc_recv(0, msg_buf);
      if (sender != (uint32_t)-1) {
         uint32_t screen_pid = user_get_pid_by_gid(SCREEN_GID);
         if (screen_pid != (uint32_t)-1) {
            uint32_t reply = 0;
            user_ipc_call(screen_pid, msg_buf, &reply);
         }// if
         user_ipc_send(sender, (void *)0);
       }// if
   }// while
}// print_service_process

void create_print_process(void) {
   infon("starting print process");
   uint32_t pid = create_process((void *)print_service_process, PRINT_GID);
   struct process *proc = get_process_by_pid(pid);
   if (proc) {
      proc->sid = generate_sid(proc->gid, (uintptr_t)print_service_process);
   }// if
   successn("print process started");
}// create_print_process
