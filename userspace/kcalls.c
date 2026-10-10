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
       default:
          errorn("Unkown Kcall");
          frame->a0 = (uint32_t)-1;
          break;
   }// switch
}// kcall_analize

/* Kernel-side implementations (run in M-mode) */

uint32_t kcall_ipc_send(uint32_t dest_pid, void *message_ptr) {
   (void)dest_pid;
   (void)message_ptr;
   return 0;
}// kcall_ipc_send

uint32_t kcall_ipc_recv(uint32_t src_pid, void *message_buf) {
   (void)src_pid;
   (void)message_buf;
   return 0;
}// kcall_ipc_recv

uint32_t kcall_ipc_call(uint32_t dest_pid, void *msg, void *reply) {
   (void)dest_pid;
   (void)msg;
   return (uint32_t)(uintptr_t)reply;
}// kcall_ipc_call

/* User-side wrapper functions (called by user processes in U-mode) */

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

void test_call(void) {
   uintptr_t call;
   asm volatile("csrr %0, mepc" : "=r"(call));
   call += 4;
   asm volatile("csrw mepc, %0" :: "r"(call));
   successn("syscall working");
}// test_call

void test_user_program(void) {
   user_ipc_send(1, (void *)0);
   while (1) {
      asm volatile("nop");
   }// while
}// test_user_program
