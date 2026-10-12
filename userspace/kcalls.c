#include <stdint.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/userspace.h"

#ifdef __USE_ANTARTICA__
#include "../include/antartica.h"
extern long antartica_syscall_dispatch(struct trap_frame *frame);
#endif

void kcall_analyze(struct trap_frame *frame) {
   frame->mepc += 4;

#ifdef __USE_ANTARTICA__
   if (frame->a7 >= 1000 && frame->a7 < 1000 + LINUX_SYSCALL_COUNT) {
     frame->a7 -= 1000;
     antartica_syscall_dispatch(frame);
     return;
   }// if
#endif

   switch (frame->a7) {
      case KCALL_YIELD:
         frame->a0 = 0;
         break;
      case KCALL_IPC_SEND:
         successn("KCALL_IPC_SEND handled in kernel");
         if (current_pid != -1 && !check_gid_permission((uint32_t)current_pid, frame->a0)) {
            errorn("gid no compatibles, permission denied");
            frame->a0 = (uint32_t)-1;
            break;
         }// if check permission
         frame->a0 = kcall_ipc_send(frame->a0, (void *)frame->a1);
         break;
      case KCALL_IPC_RECV:
         frame->a0 = kcall_ipc_recv(frame->a0, (void *)frame->a1);
         break;
      case KCALL_IPC_CALL:
         if (current_pid != -1 && !check_gid_permission((uint32_t)current_pid, frame->a0)) {
            errorn("gid no compatibles, permission denied");
            frame->a0 = (uint32_t)-1;
            break;
         }// if check permission
         frame->a0 = kcall_ipc_call(frame->a0, (void *)frame->a1, (void *)frame->a2);
         break;
      case PKCALL_MAP_DEVICE: {
         struct process *curr = get_process_by_pid((uint32_t)current_pid);
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
         errorn("Unkown kcall");
         frame->a0 = (uint32_t)-1;
         break;
   }// switch
}// kcall_analyze

/* Kernel */

uint32_t kcall_ipc_send(uint32_t dest_pid, void *message_ptr) {
   struct process *curr = get_process_by_pid((uint32_t)current_pid);
   struct process *dest = get_process_by_pid(dest_pid);
   if (!dest || !curr) {
      return (uint32_t)-1;
   }// if
   if (dest->state == PROCESS_BLOCKED_RECV) {
      dest->ipc_msg_buf = message_ptr;
      dest->state = PROCESS_READY;
      dest->frame->a0 = (uint32_t)current_pid;
      return 0;
   } else {
      curr->state = PROCESS_BLOCKED_SEND;
      curr->ipc_target_pid = dest_pid;
      curr->ipc_msg_buf = message_ptr;
      return 0;
   }// if
}// kcall_ipc_send

uint32_t kcall_ipc_recv(uint32_t src_pid, void *message_buf) {
   struct process *curr = get_process_by_pid((uint32_t)current_pid);
   if (!curr) {
      return (uint32_t)-1;
   }// if
   for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
      if (processes[i].state == PROCESS_BLOCKED_SEND && processes[i].ipc_target_pid == curr->pid) {
         if (src_pid == 0 || processes[i].pid == src_pid) {
            curr->ipc_msg_buf = processes[i].ipc_msg_buf;
            processes[i].state = PROCESS_READY;
            processes[i].frame->a0 = 0;
            return processes[i].pid;
         }// if match
      }// if blocked sender
   }// for
   curr->state = PROCESS_BLOCKED_RECV;
   curr->ipc_msg_buf = message_buf;
   return (uint32_t)-1;
}// kcall_ipc_recv

uint32_t kcall_ipc_call(uint32_t dest_pid, void *msg, void *reply) {
   kcall_ipc_send(dest_pid, msg);
   struct process *curr = get_process_by_pid((uint32_t)current_pid);
   if (curr) {
      curr->state = PROCESS_BLOCKED_RECV;
      curr->ipc_msg_buf = reply;
   }// if
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
