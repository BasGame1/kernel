#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "../include/kernel.h"
#include "../include/memory.h"
#include "../include/userspace.h"
#include "../include/hardware.h"
#include "../include/gid.h"

#ifdef __DISABLE_GID810_PKCALL__
   #define MAX_DRIVER_GID 8
#else
   #define MAX_DRIVER_GID 16
#endif

#ifndef PTE_RW
   #define PTE_RW 0x06
#endif
#ifndef PTE_U
   #define PTE_U 0x10
#endif

uint32_t generate_sid(uint32_t gid, uintptr_t entry) {
   static uint32_t sid_seed = 0xA5A5E711;
   sid_seed = sid_seed * 1103515245 + 12345;
   return (sid_seed ^ (gid << 16) ^ (uint32_t)entry);
}// generate_sid

bool verify_sid(struct process *current_process, uint32_t sid) {
   if (current_process != NULL && current_process->sid == sid) {
      return true;
   } else {
      return false;
   }// if
}// verify_sid

bool verify_pkcall(uint32_t gid, uint32_t sid) {
   if (gid == 0 || gid >= MAX_DRIVER_GID) {
      errorn("An incorrect gid tried to make a pkcall");
      return false;
   }// if

   if (current_pid == -1) {
      errorn("pkcall from invalid process id");
      return false;
   }// if

   struct process *current = get_process_by_pid((uint32_t)current_pid);
   if (current != NULL && current->gid == gid && verify_sid(current, sid)) {
      successn("pkcall sender passed all security checks");
      return true;
   } else {
      errorn("unverified or incorrect sid");
      return false;
   }// if
}// verify_pkcall

uint32_t pkcall_map_device(uintptr_t phys_addr, uintptr_t virt_addr) {
   if (phys_addr < SCREEN_ADDRESS || (phys_addr >= START_MEMORY && phys_addr < END_MEMORY)) {
      errorn("pkcall: invalid device physical address");
      return (uint32_t)-1;
   }// if
   if (virt_addr >= START_MEMORY && virt_addr < END_MEMORY) {
      errorn("pkcall: invalid device virtual address (collides with kernel)");
      return (uint32_t)-1;
   }// if
   extern uintptr_t *root_directory;
   if (!root_directory) {
      errorn("pkcall: page table not ready");
      return (uint32_t)-1;
   }// if

   map_virtual_memory(root_directory, virt_addr, phys_addr, (PTE_RW | PTE_U));
#if defined(__RISCV__) || defined(__riscv)
   asm volatile("sfence.vma zero, zero");
#elif defined(__X86__) || defined(__i386__) || defined(__x86_64__)
   asm volatile("" ::: "memory");
#endif
   successn("mapped physical with virtual via pkcall");
   return 0;
}// pkcall_map_device
