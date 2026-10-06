#ifndef __KERNEL_H
#define __KERNEL_H

extern void trap_vector();
void trap_handler();
void init_traps();

#endif
