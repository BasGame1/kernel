#ifndef __MEMORY_H
#define __MEMORY_H

#include <stdint.h>

#define PAGE 4096
#if defined(__X86__) || defined(__i386__) || defined(__x86_64__)
#define START_MEMORY 0x00100000
#define END_MEMORY 0x08000000
#elif defined(__ARM__) || defined(__arm__) || defined(__aarch64__)
#define START_MEMORY 0x40000000
#define END_MEMORY 0x48000000
#else
#define START_MEMORY 0x80000000
#define END_MEMORY 0x88000000
#endif

#define PTE_V 0x01
#define PTE_R 0x02
#define PTE_W 0x04
#define PTE_X 0x08
#define PTE_RW (PTE_R | PTE_W)
#define PTE_RX (PTE_R | PTE_X)
#define PTE_RWX 0x0E
#define PTE_U 0x10

extern uintptr_t *root_directory;
void map_virtual_memory(uintptr_t *root_directory, uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags);

#endif
