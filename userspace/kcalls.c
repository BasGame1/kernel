#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "../include/memory.h"
#include "../include/kernel.h"
#include "../include/userspace.h"

#ifdef __USE_ANTARTICA__
#include "../include/antartica.h"
extern long antartica_syscall_dispatch(struct trap_frame *frame);
#endif

#define IPC_SAFE_MSG_SIZE 64

static inline bool is_valid_user_buf(const void *ptr, size_t size) {
   if (__builtin_expect(!ptr, 0)) {
      return false;
   }// if
   uintptr_t addr = (uintptr_t)ptr;
   if (__builtin_expect(addr >= START_MEMORY && addr < END_MEMORY, 0)) {
      return false;
   }// if
   if (__builtin_expect(addr + size < addr, 0)) {
      return false;
   }// if
   return true;
}// is_valid_user_buf

void kcall_analyze(struct trap_frame *frame) {
   if (__builtin_expect(!frame, 0)) {
      return;
   }// if
   frame->mepc += 4;

#ifdef __USE_ANTARTICA__
   if (frame->a7 >= 1000 && frame->a7 < 1000 + LINUX_SYSCALL_COUNT) {
      frame->a7 -= 1000;
      antartica_syscall_dispatch(frame);
      return;
   }// if
#endif
   uint32_t kcall_num = (uint32_t)frame->a7;
   if (kcall_num == KCALL_IPC_CALL) {
      if (current_pid != -1 && !check_gid_permission((uint32_t)current_pid, frame->a0)) {
         errorn("gid not compatible, permission denied");
         frame->a0 = (uint32_t)-1;
         return;
      }// if check permission
      frame->a0 = kcall_ipc_call(frame->a0, (void *)frame->a1, (void *)frame->a2);
      return;
   }
   if (kcall_num == KCALL_IPC_SEND) {
      if (current_pid != -1 && !check_gid_permission((uint32_t)current_pid, frame->a0)) {
         errorn("gid not compatible, permission denied");
         frame->a0 = (uint32_t)-1;
         return;
      }// if check permission
      frame->a0 = kcall_ipc_send(frame->a0, (void *)frame->a1);
      return;
   }
   if (kcall_num == KCALL_IPC_RECV) {
      frame->a0 = kcall_ipc_recv(frame->a0, (void *)frame->a1);
      return;
   }
   if (kcall_num == KCALL_YIELD) {
      frame->a0 = 0;
      return;
   }

   switch (kcall_num) {
      case PKCALL_MAP_DEVICE: {
         struct process *curr = get_process_by_pid_fast((uint32_t)current_pid);
         if (!curr) {
            frame->a0 = (uint32_t)-1;
            break;
         }// if check process
         uint32_t sid = frame->a2;
         if (!verify_pkcall(curr->gid, sid)) {
            errorn("pkcall rejected for invalid or incorrect sid");
            frame->a0 = (uint32_t)-1;
            break;
         }// if check pkcall
         frame->a0 = pkcall_map_device(frame->a0, frame->a1);
         break;
      }// case PKCALL_MAP_DEVICE
      case KCALL_GET_PID_BY_GID:
         frame->a0 = get_pid_by_gid(frame->a0);
         break;
      default:
         errorn("Unknown kcall");
         frame->a0 = (uint32_t)-1;
         break;
   }// switch
}// kcall_analyze

uint32_t kcall_ipc_send(uint32_t dest_pid, void *message_ptr) {
   struct process *curr = get_process_by_pid_fast((uint32_t)current_pid);
   struct process *dest = get_process_by_pid_fast(dest_pid);
   if (__builtin_expect(!dest || !curr || !is_valid_user_buf(message_ptr, IPC_SAFE_MSG_SIZE), 0)) {
      return (uint32_t)-1;
   }// if

   if (dest->state == PROCESS_BLOCKED_RECV) {
      if (dest->ipc_msg_buf && message_ptr) {
         copy_ipc_msg_64(dest->ipc_msg_buf, message_ptr);
      }// if
      dest->state = PROCESS_READY;
      if (dest->frame) {
         dest->frame->a0 = (uint32_t)current_pid;
      }// if
      if (dest_pid >= MIN_PID && dest_pid < MAX_PID) {
         direct_handoff_idx = (int)pid_to_slot[dest_pid - MIN_PID];
      }
      return 0;
   } else {
      curr->state = PROCESS_BLOCKED_SEND;
      curr->ipc_target_pid = dest_pid;
      curr->ipc_msg_buf = message_ptr;
      return 0;
   }// if
}// kcall_ipc_send

uint32_t kcall_ipc_recv(uint32_t src_pid, void *message_buf) {
   struct process *curr = get_process_by_pid_fast((uint32_t)current_pid);
   if (__builtin_expect(!curr || !is_valid_user_buf(message_buf, IPC_SAFE_MSG_SIZE), 0)) {
      return (uint32_t)-1;
   }// if

   if (src_pid != 0) {
      struct process *sender = get_process_by_pid_fast(src_pid);
      if (sender && sender->state == PROCESS_BLOCKED_SEND && sender->ipc_target_pid == curr->pid) {
         if (message_buf && sender->ipc_msg_buf) {
            copy_ipc_msg_64(message_buf, sender->ipc_msg_buf);
         }// if
         sender->state = PROCESS_READY;
         if (sender->frame) {
            sender->frame->a0 = 0;
         }// if
         return sender->pid;
      }// if match
   } else {
      for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
         if (processes[i].state == PROCESS_BLOCKED_SEND && processes[i].ipc_target_pid == curr->pid) {
            if (message_buf && processes[i].ipc_msg_buf) {
               copy_ipc_msg_64(message_buf, processes[i].ipc_msg_buf);
            }// if
            processes[i].state = PROCESS_READY;
            if (processes[i].frame) {
               processes[i].frame->a0 = 0;
            }// if
            return processes[i].pid;
         }// if blocked sender
      }// for
   }// if

   curr->state = PROCESS_BLOCKED_RECV;
   curr->ipc_msg_buf = message_buf;
   return (uint32_t)-1;
}// kcall_ipc_recv

uint32_t kcall_ipc_call(uint32_t dest_pid, void *msg, void *reply) {
   struct process *curr = get_process_by_pid_fast((uint32_t)current_pid);
   struct process *dest = get_process_by_pid_fast(dest_pid);
   if (__builtin_expect(!curr || !dest, 0)) {
      return (uint32_t)-1;
   }// if
   if (__builtin_expect(!is_valid_user_buf(msg, IPC_SAFE_MSG_SIZE), 0)) {
      return (uint32_t)-1;
   }// if
   if (__builtin_expect(reply && !is_valid_user_buf(reply, IPC_SAFE_MSG_SIZE), 0)) {
      return (uint32_t)-1;
   }// if

   /* Send phase */
   if (dest->state == PROCESS_BLOCKED_RECV) {
      if (dest->ipc_msg_buf && msg) {
         copy_ipc_msg_64(dest->ipc_msg_buf, msg);
      }
      dest->state = PROCESS_READY;
      if (dest->frame) {
         dest->frame->a0 = (uint32_t)current_pid;
      }
      /* OPTIMIZATION: Prioritize server execution immediately */
      if (dest_pid >= MIN_PID && dest_pid < MAX_PID) {
         direct_handoff_idx = (int)pid_to_slot[dest_pid - MIN_PID];
      }// if
   } else {
      kcall_ipc_send(dest_pid, msg);
   }// if
   curr->state = PROCESS_BLOCKED_RECV;
   curr->ipc_msg_buf = reply;
   return 0;
}// kcall_ipc_call

#if defined(__RISCV__) || defined(__riscv)
uint32_t user_ipc_send(uint32_t dest_pid, void *message_ptr) {
   register uintptr_t a7 asm("a7") = KCALL_IPC_SEND;
   register uintptr_t a0 asm("a0") = dest_pid;
   register uintptr_t a1 asm("a1") = (uintptr_t)message_ptr;
   asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1) : "memory");
   return (uint32_t)a0;
}// user_ipc_send

uint32_t user_ipc_recv(uint32_t src_pid, void *message_buf) {
   register uintptr_t a7 asm("a7") = KCALL_IPC_RECV;
   register uintptr_t a0 asm("a0") = src_pid;
   register uintptr_t a1 asm("a1") = (uintptr_t)message_buf;
   asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1) : "memory");
   return (uint32_t)a0;
}// user_ipc_recv

uint32_t user_ipc_call(uint32_t dest_pid, void *msg, void *reply) {
   register uintptr_t a7 asm("a7") = KCALL_IPC_CALL;
   register uintptr_t a0 asm("a0") = dest_pid;
   register uintptr_t a1 asm("a1") = (uintptr_t)msg;
   register uintptr_t a2 asm("a2") = (uintptr_t)reply;
   asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1), "r"(a2) : "memory");
   return (uint32_t)a0;
}// user_ipc_call

uint32_t user_yield(void) {
   register uintptr_t a7 asm("a7") = KCALL_YIELD;
   register uintptr_t a0 asm("a0") = 0;
   asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
   return (uint32_t)a0;
}// user_yield

uint32_t user_get_pid_by_gid(uint32_t gid) {
   register uintptr_t a7 asm("a7") = KCALL_GET_PID_BY_GID;
   register uintptr_t a0 asm("a0") = gid;
   asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
   return (uint32_t)a0;
}// user_get_pid_by_gid
#elif defined(__X86__) || defined(__i386__) || defined(__x86_64__)
uint32_t user_ipc_send(uint32_t dest_pid, void *message_ptr) {
   uintptr_t ret;
   asm volatile("int $0x80" : "=a"(ret) : "a"(KCALL_IPC_SEND), "b"(dest_pid), "c"(message_ptr) : "memory");
   return (uint32_t)ret;
}// user_ipc_send

uint32_t user_ipc_recv(uint32_t src_pid, void *message_buf) {
   uintptr_t ret;
   asm volatile("int $0x80" : "=a"(ret) : "a"(KCALL_IPC_RECV), "b"(src_pid), "c"(message_buf) : "memory");
   return (uint32_t)ret;
}// user_ipc_recv

uint32_t user_ipc_call(uint32_t dest_pid, void *msg, void *reply) {
   uintptr_t ret;
   asm volatile("int $0x80" : "=a"(ret) : "a"(KCALL_IPC_CALL), "b"(dest_pid), "c"(msg), "d"(reply) : "memory");
   return (uint32_t)ret;
}// user_ipc_call

uint32_t user_yield(void) {
   uintptr_t ret;
   asm volatile("int $0x80" : "=a"(ret) : "a"(KCALL_YIELD) : "memory");
   return (uint32_t)ret;
}// user_yield

uint32_t user_get_pid_by_gid(uint32_t gid) {
   uintptr_t ret;
   asm volatile("int $0x80" : "=a"(ret) : "a"(KCALL_GET_PID_BY_GID), "b"(gid) : "memory");
   return (uint32_t)ret;
}// user_get_pid_by_gid
#elif defined(__ARM__) || defined(__arm__) || defined(__aarch64__)
uint32_t user_ipc_send(uint32_t dest_pid, void *message_ptr) {
   register uintptr_t r7 asm("r7") = KCALL_IPC_SEND;
   register uintptr_t r0 asm("r0") = dest_pid;
   register uintptr_t r1 asm("r1") = (uintptr_t)message_ptr;
   asm volatile("svc #0" : "+r"(r0) : "r"(r7), "r"(r1) : "memory");
   return (uint32_t)r0;
}// user_ipc_send

uint32_t user_ipc_recv(uint32_t src_pid, void *message_buf) {
   register uintptr_t r7 asm("r7") = KCALL_IPC_RECV;
   register uintptr_t r0 asm("r0") = src_pid;
   register uintptr_t r1 asm("r1") = (uintptr_t)message_buf;
   asm volatile("svc #0" : "+r"(r0) : "r"(r7), "r"(r1) : "memory");
   return (uint32_t)r0;
}// user_ipc_recv

uint32_t user_ipc_call(uint32_t dest_pid, void *msg, void *reply) {
   register uintptr_t r7 asm("r7") = KCALL_IPC_CALL;
   register uintptr_t r0 asm("r0") = dest_pid;
   register uintptr_t r1 asm("r1") = (uintptr_t)msg;
   register uintptr_t r2 asm("r2") = (uintptr_t)reply;
   asm volatile("svc #0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2) : "memory");
   return (uint32_t)r0;
}// user_ipc_call

uint32_t user_yield(void) {
   register uintptr_t r7 asm("r7") = KCALL_YIELD;
   register uintptr_t r0 asm("r0") = 0;
   asm volatile("svc #0" : "+r"(r0) : "r"(r7) : "memory");
   return (uint32_t)r0;
}// user_yield

uint32_t user_get_pid_by_gid(uint32_t gid) {
   register uintptr_t r7 asm("r7") = KCALL_GET_PID_BY_GID;
   register uintptr_t r0 asm("r0") = gid;
   asm volatile("svc #0" : "+r"(r0) : "r"(r7) : "memory");
   return (uint32_t)r0;
}// user_get_pid_by_gid
#endif

void test_call(void) {
#if defined(__RISCV__) || defined(__riscv)
   uintptr_t call;
   asm volatile("csrr %0, mepc" : "=r"(call));
   call += 4;
   asm volatile("csrw mepc, %0" :: "r"(call));
#endif
   successn("syscall working");
}// test_call

void test_user_program(void) {
   printu("Hello from userspace app via print multiplexer!\n");
   while (1) {
      user_yield();
   }// while
}// test_user_program
