#include <stdint.h>

#include "../include/kernel.h"
#include "../include/userspace.h"

extern void _enteruserspace(uintptr_t user_stack_top);

void userspace_init() {
   uint32_t user_stack = (allocate_memory() + 4096);
   uint32_t kernel_trap_stack = (allocate_memory() + 4096);
   asm volatile("csrw mscratch, %0" :: "r"(kernel_trap_stack));
   infon("Starting and entering userspace");
   enter_userspace((uintptr_t)test_user_program, user_stack);
}
void enter_userspace(uintptr_t entry, uintptr_t user_stack) {
   uint32_t state;
   asm volatile("csrr %0, mstatus" : "=r"(state));
   state &= ~(3 << 11);
   state |= (1 << 7);
   asm volatile("csrw mstatus, %0" :: "r"(state));
   asm volatile("csrw mepc, %0" :: "r"(entry));
   _enteruserspace(user_stack);
}// enter_userspace
