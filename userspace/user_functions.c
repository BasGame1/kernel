#include "../include/userspace.h"
#include "../include/gid.h"

void printu(char *msg) {
   uint32_t reply = 0;
   uint32_t print_pid = user_get_pid_by_gid(PRINT_GID);
   if (print_pid != (uint32_t)-1) {
      user_ipc_call(print_pid, msg, &reply);
   }// if
}// printu

char get_charu(void) {
   uint32_t pid = user_get_pid_by_gid(KEYBOARD_GID);
   if (pid == (uint32_t)-1) {
      return 0;
   }// if
   char req = 0;
   char reply[2] = {0, 0};
   user_ipc_call(pid, &req, reply);
   return reply[0];
}// get_charu

uint32_t user_mkdir(const char *path) {
   uint32_t fs_pid = user_get_pid_by_gid(FS_GID);
   if (fs_pid == (uint32_t)-1) {
      return -1;
   }// if
   struct fs_ipc_req req;
   req.cmd = 2;
   for (uint32_t i = 0; i < 63 && path[i] != '\0'; i++) {
      req.path[i] = path[i]; req.path[i + 1] = '\0';
   }// for
   uint32_t reply = 0;
   user_ipc_call(fs_pid, &req, &reply);
   return (int)reply;
}// user_mkdir

