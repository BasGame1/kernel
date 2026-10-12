#include "../include/antartica.h"
#include "../include/gid.h"

void antartica_init(void) {
   infon("Init antartica");
   successn("Antartica initialized with 467 syscall entry table");
}// antartica_init

void antartica_process(void) {
   infon("antartica running");
   while (1) {
      struct antartica_ipc_req req;
      uint32_t sender = user_ipc_recv(0, &req);
      if (sender != (uint32_t)-1) {
         long ret = -LINUX_ENOSYS;
         if (req.syscall_nr < LINUX_SYSCALL_COUNT) {
            linux_syscall_fn handler = linux_syscall_table[req.syscall_nr];
            if (handler) {
               ret = handler(req.args[0], req.args[1], req.args[2],
                             req.args[3], req.args[4], req.args[5]);
            }// if
         }// if
         user_ipc_send(sender, (void *)(intptr_t)ret);
      }// if
   }// while
}// antartica_process

void create_antartica_process(void) {
   antartica_init();
   infon("starting antartica");
   uint32_t pid = create_process((void *)antartica_process, ANTARTICA_GID);
   struct process *proc = get_process_by_pid(pid);
   if (proc) {
      uint32_t gid = proc->gid;
      uint32_t sid = generate_sid(gid, (uintptr_t)antartica_process);
      proc->sid = sid;
   }// if
   successn("antartica created");
}// create_antartica_process

long antartica_syscall_dispatch(struct trap_frame *frame) {
   uint32_t sc = frame->a7;
   if (sc >= LINUX_SYSCALL_COUNT) {
      errorn("antartica invalid syscall index");
      frame->a0 = (uintptr_t)-LINUX_ENOSYS;
      return -LINUX_ENOSYS;
   }// if

   linux_syscall_fn handler = linux_syscall_table[sc];
   if (!handler) {
      frame->a0 = (uintptr_t)-LINUX_ENOSYS;
      return -LINUX_ENOSYS;
   }// if

   long ret = handler(frame->a0, frame->a1, frame->a2, frame->a3, frame->a4, frame->a5);
   frame->a0 = (uintptr_t)ret;
   return ret;
}// antartica_syscall_dispatch
