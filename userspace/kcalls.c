#include <stdint.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/userspace.h"

void kcall_analyze(struct trap_frame *frame) {
   frame->mepc += 4;
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
}// kcall_analize

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
   return 0;
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

/* Userspace */

uint32_t user_ipc_send(uint32_t dest_pid, void *message_ptr) {
   register uint32_t a7 asm("a7") = KCALL_IPC_SEND;
   register uint32_t a0 asm("a0") = dest_pid;
   register uint32_t a1 asm("a1") = (uint32_t)message_ptr;
   asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1) : "memory");
   return a0;
}// user_ipc_send

uint32_t user_ipc_recv(uint32_t src_pid, void *message_buf) {
   register uint32_t a7 asm("a7") = KCALL_IPC_RECV;
   register uint32_t a0 asm("a0") = src_pid;
   register uint32_t a1 asm("a1") = (uint32_t)message_buf;
   asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1) : "memory");
   return a0;
}// user_ipc_recv

uint32_t user_ipc_call(uint32_t dest_pid, void *msg, void *reply) {
   register uint32_t a7 asm("a7") = KCALL_IPC_CALL;
   register uint32_t a0 asm("a0") = dest_pid;
   register uint32_t a1 asm("a1") = (uint32_t)msg;
   register uint32_t a2 asm("a2") = (uint32_t)reply;
   asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1), "r"(a2) : "memory");
   return a0;
}// user_ipc_call

uint32_t user_yield(void) {
   register uint32_t a7 asm("a7") = KCALL_YIELD;
   register uint32_t a0 asm("a0") = 0;
   asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
   return a0;
}// user_yield

uint32_t user_get_pid_by_gid(uint32_t gid) {
   register uint32_t a7 asm("a7") = KCALL_GET_PID_BY_GID;
   register uint32_t a0 asm("a0") = gid;
   asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
   return a0;
}// user_get_pid_by_gid

void test_call(void) {
   uintptr_t call;
   asm volatile("csrr %0, mepc" : "=r"(call));
   call += 4;
   asm volatile("csrw mepc, %0" :: "r"(call));
   successn("syscall working");
}// test_call

void test_user_program(void) {
   printu("Hello from userspace app via print multiplexer!\n");
   while (1) {
      user_yield();
   }// while
}// test_user_program
