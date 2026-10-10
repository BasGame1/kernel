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

#define KCALL_IPC_SEND 1
#define KCALL_IPC_RECV 2
#define KCALL_IPC_CALL 3

struct trap_frame {
   uint32_t mepc;
   uint32_t ra;
   uint32_t sp;
   uint32_t gp;
   uint32_t tp;
   uint32_t t0;
   uint32_t t1;
   uint32_t t2;
   uint32_t s0;
   uint32_t s1;
   uint32_t a0;
   uint32_t a1;
   uint32_t a2;
   uint32_t a3;
   uint32_t a4;
   uint32_t a5;
   uint32_t a6;
   uint32_t a7;
   uint32_t s2;
   uint32_t s3;
   uint32_t s4;
   uint32_t s5;
   uint32_t s6;
   uint32_t s7;
   uint32_t s8;
   uint32_t s9;
   uint32_t s10;
   uint32_t s11;
   uint32_t t3;
   uint32_t t4;
   uint32_t t5;
   uint32_t t6;
};
void kcall_analyze(struct trap_frame *frame);
uint32_t kcall_ipc_send(uint32_t dest_pid, void *message_ptr);
uint32_t kcall_ipc_recv(uint32_t src_pid, void *message_buf);
uint32_t kcall_ipc_call(uint32_t dest_pid, void *msg, void *reply);

#define MAX_PROCESSES 16    
#define KCALL_YIELD 0
#define KCALL_IPC_SEND 1
#define KCALL_IPC_RECV 2
#define KCALL_IPC_CALL 3
#define SYSTEM_GID 0
#define MIN_ID 64
#define MAX_ID 1024

typedef enum {
   PROCESS_UNUSED = 0,
   PROCESS_READY,
   PROCESS_RUNNING,
   PROCESS_BLOCKED_SEND,
   PROCESS_BLOCKED_RECV
} p_state_t;
struct process {
   uint32_t pid;
   uint32_t gid;
   p_state_t state;
   uintptr_t kernel_stack;
   uintptr_t user_stack;
   struct trap_frame *frame;
   uint32_t ipc_target_pid;
   void *ipc_msg_buf;
};
struct trap_frame *schedule(struct trap_frame *current_frame);
struct process processes[MAX_PROCESSES];

extern bool check_gid_permission(uint32_t sender_pid, uint32_t receiver_pid);
extern uint32_t create_process(void *entry, uint32_t gid);
extern uint32_t get_free_pid();
extern struct get_proccess_by_pid(uint32_t pid);
extern void schedule(void *current_frame);

#endif
