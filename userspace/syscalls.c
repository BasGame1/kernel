#include <stdint.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/userspace.h"

uint32_t sys_ipc_send(uint32_t dest_pid, void *message_ptr) {
   (void)dest_pid;
   (void)message_ptr;
   return 0;
}

uint32_t sys_ipc_recv(uint32_t src_pid, void *message_buf) {
   (void)src_pid;
   (void)message_buf;
   return 0;
}

uint32_t sys_ipc_call(uint32_t dest_pid, void *msg, void *reply) {
   (void)dest_pid;
   (void)msg;
   return (uint32_t)(uintptr_t)reply;
}

void sys_map_device(uintptr_t phys_addr, uintptr_t virt_addr) {
   (void)phys_addr;
   (void)virt_addr;
}

void test_call(void) {
   uintptr_t call;
   asm volatile("csrr %0, mepc" : "=r"(call));
   call += 4;
   asm volatile("csrw mepc, %0" :: "r"(call));
   successn("syscall working");
}
void test_user_program(void) {
   asm volatile("ecall");
   while (1) {
      asm volatile("nop");
   }// while
   successn("User program calls worked");
}// test_user_program
