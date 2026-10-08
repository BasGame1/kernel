#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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
extern void set_mtimecmp(void);

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
extern void fataln(char *reason);
extern void debugn(char *reason);
extern void successn(char *reason);
extern void infon(char *reason);
extern void errorn(char *reason);
extern void printn(char *message);
extern void start_screen(void);
extern void print_character(char *message);

extern void print_asccii_art(void);

extern char *concat(char *hardcoded_text, char *reason);
extern int strcmp(const char *s1, const char *s2);
extern size_t parse_number(char *str);
extern uint32_t round_page(uint32_t number, uint32_t page);
extern void print_hex(uint32_t val);
extern void print_dec(uint32_t val);

extern char *get_input(void);
extern char get_char(void);
extern bool test_keyboard(void);
extern void print_input(void);

#endif
