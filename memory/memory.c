#include <stdint.h>
#include <stddef.h>

#include "../include/memory.h"
#include "../include/kernel.h"

#include "../utils/round.c"
#include "../screen/status.c"

extern int _end;
int start_memory = &_end;

uintptr_t current_block;
uintptr_t next_block;
uintptr_t *head = 0;
uintptr_t *actual_page;

uintptr_t get_end_of_memory() {
   uintptr_t value;
   asm volatile("csrs %0, t0":"=r"(value))
   return value;
}// get_end_of_memory

void divide_memory(uintptr_t start, uintptr_t end) {
   /* If anyone reads this block I am sorry if you do not understand
    * The var names are horrible, but I could not come up with a better name
    */
   current_block = start;
   while (current_block < end) {
      uintptr_t *current_page = (uintptr_t *)current_block;
      *current_page = head;
      head = current_block;
      current_block += 0x1000; // 4096
   }// while loop to divide the memory
}// divide_memory

void free_memory(uintptr_t *current_full_page) {
   *current_full_page = head;
   head = current_full_page;
}// free_memory

uintptr_t allocate_memory() {
   if ( head == 0 ) {
      fatal("Out of Memory");
      //TODO, add case for oom
   }// if to see if oom
   uintptr_t free_page = head;
   uintptr_t *next_free_page = (uintptr_t*) head;
   head = *next_free_page;
   return free_page;
}// allocate_memory

void init_memory() {
   uintptr_t start_memory_rounded = round(start_memory, PAGE);
   uintptr_t end_memory = get_end_of_memory();
   divide_memory(start_memory_rounded, end_memory);
}// init_memory
