#include "../include/userspace.h"
#include "../include/gid.h"

void printu(char *msg) {
   uint32_t reply = 0;
   user_ipc_call(PRINT_GID, msg, &reply);
}// print
