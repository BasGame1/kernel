#include "../include/userspace.h"
#include "../include/gid.h"

static uint32_t cached_print_pid = (uint32_t)-1;
static uint32_t cached_kbd_pid = (uint32_t)-1;
static uint32_t cached_fs_pid = (uint32_t)-1;

void printu(char *msg) {
   uint32_t reply = 0;
   if (cached_print_pid == (uint32_t)-1) {
      cached_print_pid = user_get_pid_by_gid(PRINT_GID);
   }
   if (cached_print_pid != (uint32_t)-1) {
      uint32_t ret = user_ipc_call(cached_print_pid, msg, &reply);
      if (ret == (uint32_t)-1) {
         cached_print_pid = user_get_pid_by_gid(PRINT_GID);
         if (cached_print_pid != (uint32_t)-1) {
            user_ipc_call(cached_print_pid, msg, &reply);
         }// if
      }
   }// if
}// printu

char get_charu(void) {
   if (cached_kbd_pid == (uint32_t)-1) {
      cached_kbd_pid = user_get_pid_by_gid(KEYBOARD_GID);
   }
   if (cached_kbd_pid == (uint32_t)-1) {
      return 0;
   }// if
   char req = 0;
   char reply[2] = {0, 0};
   uint32_t ret = user_ipc_call(cached_kbd_pid, &req, reply);
   if (ret == (uint32_t)-1) {
      cached_kbd_pid = user_get_pid_by_gid(KEYBOARD_GID);
      if (cached_kbd_pid != (uint32_t)-1) {
         user_ipc_call(cached_kbd_pid, &req, reply);
      }
   }
   return reply[0];
}// get_charu

uint32_t user_mkdir(const char *path) {
   if (cached_fs_pid == (uint32_t)-1) {
      cached_fs_pid = user_get_pid_by_gid(FS_GID);
   }
   if (cached_fs_pid == (uint32_t)-1) {
      return -1;
   }// if
   struct fs_ipc_req req;
   req.cmd = 2;
   for (uint32_t i = 0; i < 63 && path[i] != '\0'; i++) {
      req.path[i] = path[i]; req.path[i + 1] = '\0';
   }// for
   uint32_t reply = 0;
   uint32_t ret = user_ipc_call(cached_fs_pid, &req, &reply);
   if (ret == (uint32_t)-1) {
      cached_fs_pid = user_get_pid_by_gid(FS_GID);
      if (cached_fs_pid != (uint32_t)-1) {
         user_ipc_call(cached_fs_pid, &req, &reply);
      }
   }
   return (int)reply;
}// user_mkdir
