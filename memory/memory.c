#include <stdint.h>
#include <stddef.h>

#include "../include/memory.h"
#include "../include/kernel.h"

extern uint32_t round_page(uint32_t number, uint32_t page);
extern void fatal(char *reason);

extern int _end;
uintptr_t start_memory = (uintptr_t)&_end;

uintptr_t current_block;
uintptr_t next_block;
uintptr_t *head = 0;
uintptr_t *actual_page;
static uintptr_t mem_start_bound = 0;
static uintptr_t mem_end_bound = 0;

#define MAX_PAGES ((END_MEMORY - START_MEMORY) / PAGE)
#define BITMAP_WORDS ((MAX_PAGES + 31) / 32)
static uint32_t alloc_bitmap[BITMAP_WORDS];

static inline uint32_t page_to_index(uintptr_t addr) {
   return (uint32_t)((addr - START_MEMORY) / PAGE);
}

static inline bool bitmap_is_allocated(uintptr_t addr) {
   uint32_t idx = page_to_index(addr);
   return (alloc_bitmap[idx / 32] & (1U << (idx % 32))) != 0;
}

static inline void bitmap_mark_allocated(uintptr_t addr) {
   uint32_t idx = page_to_index(addr);
   alloc_bitmap[idx / 32] |= (1U << (idx % 32));
}

static inline void bitmap_mark_free(uintptr_t addr) {
   uint32_t idx = page_to_index(addr);
   alloc_bitmap[idx / 32] &= ~(1U << (idx % 32));
}

#ifdef __QEMU__
uintptr_t get_end_of_memory() {
   infon("QEMU: Memory value hardcoded");
   return END_MEMORY;
}// get_end_of_memory
#else
uintptr_t get_end_of_memory() {
   infon("Testing the end of memory");
   uintptr_t probe_address = (uintptr_t)&_end;
   const uintptr_t step = 1024 * 1024 * 4;
   const uint32_t pattern = 0xAA55AA55;
   while (probe_address < END_MEMORY) {
      volatile uint32_t *ptr = (volatile uint32_t *)probe_address;
      uint32_t original = *ptr;
      *ptr = pattern;
      if (*ptr != pattern) {
         break;
      }// if
      *ptr = original;
      probe_address += step;
   }// while
   successn("Reached the end of memory");
   return probe_address;
}// get_end_of_memory
#endif

void divide_memory(uintptr_t start, uintptr_t end) {
   infon("Dividing memory in pages");
   mem_start_bound = start;
   mem_end_bound = end;
   head = 0;
   for (uint32_t i = 0; i < BITMAP_WORDS; i++) {
      alloc_bitmap[i] = 0;
   }// for

   current_block = start;
   while (current_block + PAGE <= end) {
      uintptr_t *current_page = (uintptr_t *)current_block;
      *current_page = (uintptr_t)head;
      head = current_page;
      current_block += PAGE;
   }// while
   successn("Memory divided in pages");
}// divide_memory

void free_memory(uintptr_t *current_full_page) {
   if (__builtin_expect(!current_full_page, 0)) {
      errorn("free_memory: NULL pointer passed");
      return;
   }// if
   uintptr_t addr = (uintptr_t)current_full_page;
   if (__builtin_expect((addr & (PAGE - 1)) != 0, 0)) {
      errorn("free_memory: unaligned page pointer rejected");
      return;
   }// if
   if (__builtin_expect(addr < mem_start_bound || addr >= mem_end_bound, 0)) {
      errorn("free_memory: out of bounds pointer rejected");
      return;
   }// if
   if (__builtin_expect(!bitmap_is_allocated(addr), 0)) {
      errorn("free_memory: double free attempt detected");
      return;
   }// if

   bitmap_mark_free(addr);
   *current_full_page = (uintptr_t)head;
   head = current_full_page;
}// free_memory

uintptr_t allocate_memory() {
   if (__builtin_expect(head == 0, 0)) {
      errorn("Out of Memory");
      return 0;
   }// if
   uintptr_t free_page = (uintptr_t)head;
   if (__builtin_expect(free_page < mem_start_bound || free_page >= mem_end_bound || (free_page & (PAGE - 1)) != 0, 0)) {
      errorn("allocate_memory: free list corruption detected");
      head = 0;
      return 0;
   }// if
   uintptr_t *next_free_page = head;
   head = (uintptr_t *)(*next_free_page);

   bitmap_mark_allocated(free_page);

   memset((void *)free_page, 0, PAGE);
   return free_page;
}// allocate_memory

void init_memory() {
   uintptr_t start_memory_rounded = round_page((uint32_t)start_memory, PAGE);
   uintptr_t end_memory = get_end_of_memory();
   divide_memory(start_memory_rounded, end_memory);
}// init_memory
