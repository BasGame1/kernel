#include <stddef.h>
#include <stdint.h>

#ifndef __USERSPACE_H
#define __USERSPACE_H

#define print(message) print_character(message)

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
extern void clear_screen();
extern void print_character(char *message);

extern void print_asccii_art(void);
struct ext2_dir_entry;
extern bool dir_entry_matches(const struct ext2_dir_entry *entry, const char *name, uint32_t name_len);
extern char *concat(char *hardcoded_text, char *reason);
extern void *memcpy(void *dest, const void *src, size_t n);
extern int strcmp(const char *s1, const char *s2);
extern void *memset(void *dest, int val, size_t len);
extern size_t parse_number(char *str);
extern uint32_t round_page(uint32_t number, uint32_t page);
extern void print_hex(uint32_t val);
extern void print_dec(uint32_t val);

extern char *get_input(void);
extern char get_char(void);
extern bool test_keyboard(void);
extern void print_input(void);

extern int update(const char *path, uint32_t mode, uint32_t mtime);
extern int remove(const char *path);
extern int write(const char *path, const void *buffer, uint32_t count, uint32_t offset);
extern int read(const char *path, void *buffer, uint32_t count, uint32_t offset);

extern void enter_userspace(uintptr_t entry, uintptr_t user_stack_top);
extern void test_call(void);

extern uint32_t sys_ipc_send(uint32_t dest_pid, void *message_ptr);
extern uint32_t sys_ipc_recv(uint32_t src_pid, void *message_buf);
extern uint32_t sys_ipc_call(uint32_t dest_pid, void *msg, void *reply);
extern void sys_map_device(uintptr_t phys_addr, uintptr_t virt_addr);

extern void test_user_program(void);
extern void userspace_init(void);
#endif
