#include "../include/userspace.h"
#include "../include/gid.h"

void printu(char *msg) {
   uint32_t reply = 0;
   uint32_t print_pid = user_get_pid_by_gid(PRINT_GID);
   if (print_pid != (uint32_t)-1) {
      user_ipc_call(print_pid, msg, &reply);
   }// if
}// printu
