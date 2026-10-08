#include <stdint.h>

#ifndef __KERNEL_H
#define __KERNEL_H

uintptr_t mtime;
uintptr_t mtimemcp;

#define print(message) print_character(message)
#define set_csrs(reg, bit) asm volatile("csrs " #reg ", %0" :: "r"(bit))

extern void trap_vector();
extern void trap_handler();
extern void init_traps();
typedef void (*handler_t)(void);
handler_t interrupt_handlers[16];
handler_t exception_handlers[16];

extern void init_memory(void);
extern void init_virtual_memory(void);
extern void init_mtimecmp(void);
extern void start_screen(void);
extern void print_character(char *message);
extern void unknown_error(void);
extern void handle_illegal_instruction(void);
extern void handle_breakpoint(void);
extern void handle_load_access_error(void);
extern void handle_store_access_error(void);
extern void handle_clock_error(void);
extern void handle_load_page_error(void);
extern void handle_store_page_error(void);
extern void handle_instruction_page_error(void);
extern void fatal(char *reason);
extern void debug(char *reason);
extern void success(char *reason);
extern void info(char *reason);
extern void error(char *reason);
extern void print_asccii_art(void);

#endif
