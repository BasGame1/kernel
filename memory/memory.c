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
    while (probe_address < 0xFF000000) {
       volatile uint32_t *ptr = (volatile uint32_t *)probe_address;
       uint32_t original = *ptr;
       *ptr = pattern;
       if (*ptr != pattern) {
          break;
        }
        *ptr = original;
        probe_address += step;
    }// while
    successn("Reached the end of memory");
    return probe_address;
  }// get_end_of_memory

/*
 *   uintptr_t get_end_of_memory() {
 *      uintptr_t value;
 *      asm volatile("mv %0, t0" : "=r"(value));
 *      return value;
 *   }// get_end_of_memory
 */ 
#endif
void divide_memory(uintptr_t start, uintptr_t end) {
   /* If anyone reads this block I am sorry if you do not understand
    * The var names are horrible, but I could not come up with a better name
    */
   infon("Dividing memory in pages");
   current_block = start;
   while (current_block < end) {
      #ifdef __DEBUG__
          debug("Looping, actual block is: ");
          debugn(current_block);
      #endif
      uintptr_t *current_page = (uintptr_t *)current_block;
      *current_page = (uintptr_t) head;
      head = (uintptr_t *) current_block;
      current_block += 0x1000; // 4096
   }// while loop to divide the memory
   successn("Memory divided in pages");
}// divide_memory

void free_memory(uintptr_t *current_full_page) {
   infon("Freeing memory");
   *current_full_page = (uintptr_t) head;
   head = current_full_page;
   successn("Freeing memory was successfull");
}// free_memory

uintptr_t allocate_memory() {
   infon("Allocating memory");
   if ( head == 0 ) {
      errorn("Out of Memory");
      //TODO, add case for oom
   }// if to see if oom
   uintptr_t free_page = (uintptr_t) head;
   uintptr_t *next_free_page = head;
   head = (uintptr_t *) *next_free_page;
   successn("Allocated memory successfully");
   return free_page;
}// allocate_memory

void init_memory() {
   uintptr_t start_memory_rounded = round_page(start_memory, PAGE);
   uintptr_t end_memory = get_end_of_memory();
   divide_memory(start_memory_rounded, end_memory);
}// init_memory
