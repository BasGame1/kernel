#ifndef __KERNEL_H
#define __KERNEL_H

extern void trap_vector();
void trap_handler();
void init_traps();
typedef void (*handler_t)(void);
handler_t interrupt_handlers[16];
handler_t exception_handlers[16];

#endif
