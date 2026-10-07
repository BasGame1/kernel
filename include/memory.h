#ifndef __MEMORY_H
#define __MEMORY_H

#define PAGE 4096
#define START_MEMORY 0x80000000

extern int _end;
int start_memory = &_end;

uintptr_t current_block;
uintptr_t next_block;
uintptr_t *head = 0;
uintptr_t *actual_page;

#ifdef
