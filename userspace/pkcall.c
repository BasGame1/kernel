#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
    
#include "../include/kernel.h"
#include "../include/memory.h"
#include "../include/userspace.h"
    
#ifdef __DISABLE_GID810_PKCALL__
    #define MAX_DRIVER_GID 8
#else
    #define MAX_DRIVER_GID 11
#endif
    
#ifdef __VERIFY_PKCALL__
    
    #ifdef __USE_SID_PROTECTION__
    
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
    
    #else
    
    uint32_t generate_sid(uint32_t gid, uintptr_t entry) {
       (void)gid;
       (void)entry;
       return 0;
    }// generate_sid
    
     bool verify_sid(struct process *current_process, uint32_t sid) {
        (void)current_process;
        (void)sid;
        return true;
     }// verify_sid
    
#endif
    
     bool verify_pkcall(uint32_t gid, uint32_t sid) {
        if (gid >= MAX_DRIVER_GID) {
           errorn("An incorrect gid tried to make a pkcall");
           return false;
         }// if check gid
    
        struct process *current = get_process_by_pid((uint32_t)current_pid);
        if (current != NULL && verify_sid(current, sid)) {
           successn("pkcall sender passed all security checks");
           return true;
        } else {
           errorn("unverified or incorrect sid");
           return false;
        }// if security check
     }// verify_pkcall
    
#else

   uint32_t generate_sid(uint32_t gid, uintptr_t entry) {
      (void)gid;
      (void)entry;
      return 0;
   }// generate_sid

   bool verify_pkcall(uint32_t gid, uint32_t sid) {
      (void)gid;
      (void)sid;
      return true;
   }// verify_pkcall

#endif

uint32_t pkcall_map_device(uintptr_t phys_addr, uintptr_t virt_addr) {
   extern uintptr_t *root_directory;
   map_virtual_memory(root_directory, virt_addr, phys_addr, (PTE_RWX | PTE_U));
   asm volatile("sfence.vma zero, zero");
   successn("mapped phyical with virtual via pkcall");
   return 0;
}// pkcall_map_device
