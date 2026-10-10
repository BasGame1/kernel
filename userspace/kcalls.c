#include <stdint.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/userspace.h"

void kcall_analyze(struct trap_frame *frame) {
   frame->mepc += 4;
   switch (frame->a7) {
       case KCALL_IPC_SEND:
          successn("KCALL_IPC_SEND handled in kernel");
          frame->a0 = kcall_ipc_send(frame->a0, (void *)frame->a1);
          break;
        case KCALL_IPC_RECV:
           frame->a0 = kcall_ipc_recv(frame->a0, (void *)frame->a1);
           break;
        case KCALL_IPC_CALL:
           frame->a0 = kcall_ipc_call(frame->a0, (void *)frame->a1, (void *)frame->a2);
           break;
        default:
           errorn("Unkown Kcall");
           frame->a0 = (uint32_t)-1;
           break;
   }// switch
}// kcall_analize
uint32_t kcall_ipc_send(uint32_t dest_pid, void *message_ptr) {
   (void)dest_pid;
   (void)message_ptr;
   return 0;
}

uint32_t kcall_ipc_recv(uint32_t src_pid, void *message_buf) {
   (void)src_pid;
   (void)message_buf;
   return 0;
}

uint32_t kcall_ipc_call(uint32_t dest_pid, void *msg, void *reply) {
   (void)dest_pid;
   (void)msg;
   return (uint32_t)(uintptr_t)reply;
}

void test_call(void) {
   uintptr_t call;
   asm volatile("csrr %0, mepc" : "=r"(call));
   call += 4;
   asm volatile("csrw mepc, %0" :: "r"(call));
   successn("syscall working");
}
void test_user_program(void) {
   register uint32_t a7 asm("a7") = KCALL_IPC_SEND;
   register uint32_t a0 asm("a0") = 1;
   register uint32_t a1 asm("a1") = 0;
   asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1) : "memory");
   while (1) {
      asm volatile("nop");
   }// while
}// test_user_program
