#include <stdint.h>
#include <stddef.h>

#include "../include/memory.h"
#include "../include/hardware.h"

extern void *memset(void *dest, int val, size_t len);
extern uintptr_t allocate_memory(void);
extern uintptr_t get_end_of_memory(void);
extern uintptr_t start_memory;

/* Implementation of SV32 for virtual memory */

void map_virtual_memory(uintptr_t *root_directory, uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags) {
   uint32_t vpn = (virtual_address >> 22) & 0x3FF;
   uint32_t pte = root_directory[vpn];
   if ((pte & 1) == 0 ) {
      // Level 2 does not exist
      info("Level 2 does not exist, creating\n");
      uintptr_t *level2_table = (uintptr_t *) allocate_memory();
      memset(level2_table,0,4096);
      root_directory[vpn] = ((uintptr_t)level2_table >> 2) | 1;
      pte = root_directory[vpn];
      success("Level 2 table created\n");
   } else {
      info("Level 2 table exists\n");
   }// if for the level 2 table
   uintptr_t *level2_table_pointer = (uintptr_t *) (uintptr_t) ((pte >> 10) << 12);
   uint32_t vpn0 = (virtual_address >> 12) & 0x3FF;
   level2_table_pointer[vpn0] = (physical_address >> 2) | flags | 1;
}// map_virtual_memory

void init_virtual_memory() {
   uintptr_t *root_directory = (uintptr_t *) allocate_memory();
   memset(root_directory,0,4096);
   uint32_t address = start_memory;
   uint32_t end_address = get_end_of_memory();
   while (address < end_address) {
      info("Mapping a virtual address\n");
      map_virtual_memory(root_directory, address, address, 14);
      address += 0x1000; // 4096      
   }// while
   success("Mapped the virtual addresses\n");
   
   info("Map clit to 0x02000000\n");
   map_virtual_memory(root_directory, 0x02000000, 0x02000000, 14); // map clit
   success("Mapping of clit succesfull\n");
   info("Map uart to 0x01000000\n");
   map_virtual_memory(root_directory, 0x10000000, 0x10000000, 14); // map uart
   success("Mapping of uart succesfull\n");
   
   uint32_t i = START_HARDWARE_ADDRESS;
   while (i < END_HARDWARE_ADDRESS) {
      info("Mapping virtual address of physical hardware\n");
      map_virtual_memory(root_directory, i, i, 14);
      i += 0x1000;
   } // while to loop for mapping every hardware address
   success("Successfully mapped the address of physical hardware to virtual addresses\n");
   
   info("Enabling sv32 in the registry\n");
   uint32_t ppn = (uint32_t)(((uintptr_t)root_directory) >> 12);
   uint32_t satp_value = ppn | 0x80000000;
   asm volatile("csrw satp, %0" : : "r" (satp_value));
   asm volatile("sfence.vma zero, zero");
   success("Enabled sv32 in the registry\n");
}// init_virtual_memory
