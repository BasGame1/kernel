#ifndef __KERNEL_H
#define __KERNEL_H

#define uuintptr_t mtime
#define uintptr_t mtimemcp

#define set_csrs(reg, bit) asm volatile("csrs " #reg ", %0" :: "r"(bit))

extern void trap_vector();
void trap_handler();
void init_traps();
typedef void (*handler_t)(void);
handler_t interrupt_handlers[16];
handler_t exception_handlers[16];

#endif
