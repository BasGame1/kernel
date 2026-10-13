#include <stdint.h>

#include "../include/kernel.h"
#include "../include/userspace.h"
#include "../include/memory.h"

#ifndef PTE_RW
   #define PTE_RW 0x06
#endif

extern void _enteruserspace(uintptr_t user_stack_top);

void userspace_init() {
   uintptr_t user_stack_page = allocate_memory();
   uintptr_t kernel_trap_stack_page = allocate_memory();
   if (!user_stack_page || !kernel_trap_stack_page) {
      fatal("Failed to allocate initial userspace stacks");
      return;
   }// if
   uintptr_t user_stack = user_stack_page + 4096;
   extern uintptr_t *root_directory;
   if (root_directory) {
      map_virtual_memory(root_directory, user_stack_page, user_stack_page, (PTE_RW | PTE_U));
   }// if
#if defined(__RISCV__) || defined(__riscv)
   uintptr_t kernel_trap_stack = kernel_trap_stack_page + 4096;
   asm volatile("csrw mscratch, %0" :: "r"(kernel_trap_stack));
#else
   (void)kernel_trap_stack_page;
#endif
   infon("Starting and entering userspace");
   enter_userspace((uintptr_t)test_user_program, user_stack);
}// userspace_init

void enter_userspace(uintptr_t entry, uintptr_t user_stack) {
#if defined(__RISCV__) || defined(__riscv)
   uint32_t state;
   asm volatile("csrr %0, mstatus" : "=r"(state));
   state &= ~(3 << 11);
   state |= (1 << 7);
   asm volatile("csrw mstatus, %0" :: "r"(state));
   asm volatile("csrw mepc, %0" :: "r"(entry));
#else
   (void)entry;
#endif
   #ifdef __DEBUG__
      uintptr_t entry1 = (uintptr_t)test_user_program;
      create_process((void *)entry1, 0);
      uintptr_t entry2 = (uintptr_t)test_user_program;
      create_process((void *)entry2, 1);
      uintptr_t entry3 = (uintptr_t)test_user_program;
      create_process((void *)entry3, 2);
   #endif
   _enteruserspace(user_stack);
}// enter_userspace
