#include <stdint.h>

#ifndef __KERNEL_H
#define __KERNEL_H

uintptr_t mtime;
uintptr_t mtimemcp;

#define print(message) print_character(message)
#define set_csrs(reg, bit) asm volatile("csrs " #reg ", %0" :: "r"(bit))

extern void trap_vector();
void trap_handler();
void init_traps();
typedef void (*handler_t)(void);
handler_t interrupt_handlers[16];
handler_t exception_handlers[16];

#endif
