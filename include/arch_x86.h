#include <stdint.h>
#include <stddef.h>

#ifndef __ARCH_X86_H
#define __ARCH_X86_H

#define X86_PAGE_SIZE 4096

static inline uint8_t inb(uint16_t port) {
   uint8_t ret;
   asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
   return ret;
}// inb

static inline void outb(uint16_t port, uint8_t val) {
   asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}// outb

static inline uint16_t inw(uint16_t port) {
   uint16_t ret;
   asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
   return ret;
}// inw

static inline void outw(uint16_t port, uint16_t val) {
   asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}// outw

static inline void x86_cli(void) {
   asm volatile("cli");
}// x86_cli

static inline void x86_sti(void) {
   asm volatile("sti");
}// x86_sti

struct trap_frame_x86 {
   uint32_t gs, fs, es, ds;
   uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
   uint32_t int_no, err_code;
   uint32_t eip, cs, eflags, user_esp, ss;
};

struct multiboot_module {
   uint32_t mod_start;
   uint32_t mod_end;
   uint32_t string;
   uint32_t reserved;
};

struct multiboot_info {
   uint32_t flags;
   uint32_t mem_lower;
   uint32_t mem_upper;
   uint32_t boot_device;
   uint32_t cmdline;
   uint32_t mods_count;
   uint32_t mods_addr;
};

extern uint32_t multiboot_magic;
extern uint32_t multiboot_info_ptr;

#endif
