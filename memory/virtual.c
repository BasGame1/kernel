#include <stdint.h>
#include <stddef.h>

#include "../include/memory.h"
#include "../include/hardware.h"
#include "../include/kernel.h"

#ifndef PTE_RW
   #define PTE_RW 0x06
#endif

extern void *memset(void *dest, int val, size_t len);
extern uintptr_t allocate_memory(void);
extern uintptr_t get_end_of_memory(void);
extern uintptr_t start_memory;

uintptr_t *root_directory = 0;

void map_virtual_memory(uintptr_t *root_dir, uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags) {
   if (!root_dir) {
      return;
   }// if

#if defined(__x86_64__)
   (void)flags;
   uint64_t vpn3 = (virtual_address >> 39) & 0x1FF;
   uint64_t vpn2 = (virtual_address >> 30) & 0x1FF;
   uint64_t vpn1 = (virtual_address >> 21) & 0x1FF;
   uint64_t vpn0 = (virtual_address >> 12) & 0x1FF;

   uint64_t *pml4 = (uint64_t *)root_dir;
   if ((pml4[vpn3] & 1) == 0) {
      uintptr_t pdpt = allocate_memory();
      if (!pdpt) {
         return;
      }// if
      memset((void *)pdpt, 0, 4096);
      pml4[vpn3] = (uint64_t)pdpt | 7;
   }// if

   uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[vpn3] & ~0xFFFULL);
   if ((pdpt[vpn2] & 1) == 0) {
      uintptr_t pd = allocate_memory();
      if (!pd) {
         return;
      }// if
      memset((void *)pd, 0, 4096);
      pdpt[vpn2] = (uint64_t)pd | 7;
   }// if

   uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[vpn2] & ~0xFFFULL);
   if (pd[vpn1] & 0x80) {
      uintptr_t pt = allocate_memory();
      if (!pt) {
         return;
      }// if
      uint64_t base_pa = pd[vpn1] & ~0x1FFFFFULL;
      uint64_t flags_2m = pd[vpn1] & 0xFFF & ~0x80ULL;
      uint64_t *pt_ptr = (uint64_t *)pt;
      for (int k = 0; k < 512; k++) {
         pt_ptr[k] = (base_pa + (uint64_t)k * 4096) | flags_2m;
      }// for
      pd[vpn1] = (uint64_t)pt | 7;
   }// if
   if ((pd[vpn1] & 1) == 0) {
      uintptr_t pt = allocate_memory();
      if (!pt) {
         return;
      }// if
      memset((void *)pt, 0, 4096);
      pd[vpn1] = (uint64_t)pt | 7;
   }// if

   uint64_t *pt = (uint64_t *)(uintptr_t)(pd[vpn1] & ~0xFFFULL);
   pt[vpn0] = (physical_address & ~0xFFFULL) | 7;
   asm volatile("invlpg (%0)" : : "r"(virtual_address) : "memory");
#elif defined(__64BIT__) || defined(__riscv64) || defined(__aarch64__)
   uint64_t vpn2 = (virtual_address >> 30) & 0x1FF;
   uint64_t vpn1 = (virtual_address >> 21) & 0x1FF;
   uint64_t vpn0 = (virtual_address >> 12) & 0x1FF;

   uint64_t pte2 = root_dir[vpn2];
   if ((pte2 & PTE_V) == 0) {
      uintptr_t *mid_table = (uintptr_t *)allocate_memory();
      if (!mid_table) {
         errorn("Level 1 allocation failed: out of memory");
         return;
      }// if
      memset(mid_table, 0, 4096);
      root_dir[vpn2] = (((uintptr_t)mid_table >> 12) << 10) | PTE_V;
      pte2 = root_dir[vpn2];
   }// if

   uintptr_t *mid_table_ptr = (uintptr_t *)(((pte2 >> 10) & 0xFFFFFFFFFFF) << 12);
   uint64_t pte1 = mid_table_ptr[vpn1];
   if ((pte1 & PTE_V) == 0) {
      uintptr_t *leaf_table = (uintptr_t *)allocate_memory();
      if (!leaf_table) {
         errorn("Level 0 allocation failed: out of memory");
         return;
      }// if
      memset(leaf_table, 0, 4096);
      mid_table_ptr[vpn1] = (((uintptr_t)leaf_table >> 12) << 10) | PTE_V;
      pte1 = mid_table_ptr[vpn1];
   }// if

   uintptr_t *leaf_table_ptr = (uintptr_t *)(((pte1 >> 10) & 0xFFFFFFFFFFF) << 12);
   leaf_table_ptr[vpn0] = (((physical_address >> 12) & 0xFFFFFFFFFFF) << 10) | flags | PTE_V;
#else
   uint32_t vpn = (virtual_address >> 22) & 0x3FF;
   uint32_t pte = (uint32_t)root_dir[vpn];
   if ((pte & PTE_V) == 0) {
      infon("Level 2 does not exist, creating");
      uintptr_t *level2_table = (uintptr_t *)allocate_memory();
      if (!level2_table) {
         errorn("Level 2 allocation failed: out of memory");
         return;
      }// if
      memset(level2_table, 0, 4096);
      root_dir[vpn] = (((uintptr_t)level2_table >> 12) << 10) | PTE_V;
      pte = (uint32_t)root_dir[vpn];
      successn("Level 2 table created");
   }// if
   uint32_t *level2_table_pointer = (uint32_t *)((uintptr_t)((pte >> 10) << 12));
   uint32_t vpn0 = (virtual_address >> 12) & 0x3FF;
   level2_table_pointer[vpn0] = ((physical_address >> 12) << 10) | flags | PTE_V;
#endif
}// map_virtual_memory

void init_virtual_memory() {
#if defined(__x86_64__)
   extern uint64_t pml4_table[512];
   root_directory = (uintptr_t *)pml4_table;
   successn("x86_64 virtual memory initialized");
#else
   root_directory = (uintptr_t *)allocate_memory();
   if (!root_directory) {
      fatal("Failed to allocate root page table");
      return;
   }// if
   memset(root_directory, 0, 4096);
   uint32_t address = START_MEMORY;
   uint32_t end_address = get_end_of_memory();
   while (address < end_address) {
      map_virtual_memory(root_directory, address, address, PTE_RWX);
      address += 0x1000;
   }// while
   successn("Mapped the virtual addresses");
   
   infon("Map clint to 0x02000000");
   map_virtual_memory(root_directory, 0x02000000, 0x02000000, PTE_RW);
   successn("Mapping of clint successful");
   infon("Map uart to 0x10000000");
   map_virtual_memory(root_directory, 0x10000000, 0x10000000, PTE_RW);
   successn("Mapping of uart successful");
   
   uint32_t i = START_HARDWARE_ADDRESS;
   while (i < END_HARDWARE_ADDRESS) {
      map_virtual_memory(root_directory, i, i, PTE_RW);
      i += 0x1000;
   }// while
   successn("Successfully mapped the address of physical hardware to virtual addresses");
   
#if defined(__RISCV__) || defined(__riscv)
   #if defined(__64BIT__) || defined(__riscv64)
      info("Enabling sv39 in the registry");
      uint64_t ppn = (((uintptr_t)root_directory) >> 12);
      uint64_t satp_value = (8ULL << 60) | ppn;
      asm volatile("csrw satp, %0" : : "r" (satp_value));
      asm volatile("sfence.vma zero, zero");
      successn("Enabled sv39 in the registry");
   #else
      info("Enabling sv32 in the registry");
      uint32_t ppn = (uint32_t)(((uintptr_t)root_directory) >> 12);
      uint32_t satp_value = ppn | 0x80000000;
      asm volatile("csrw satp, %0" : : "r" (satp_value));
      asm volatile("sfence.vma zero, zero");
      successn("Enabled sv32 in the registry");
   #endif
#elif defined(__X86__) || defined(__i386__)
   info("x86 architecture paging initialized");
#endif
#endif
}// init_virtual_memory
