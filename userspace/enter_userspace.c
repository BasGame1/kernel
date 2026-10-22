#include <stdint.h>

#include "../include/kernel.h"
#include "../include/userspace.h"
#include "../include/memory.h"

#if defined(__X86__) || defined(__i386__) || defined(__x86_64__)
#include "../include/arch_x86.h"
#endif

#ifdef __USE_ANTARTICA__
#include "../include/antartica.h"
#endif

#ifndef PTE_RW
   #define PTE_RW 0x06
#endif

extern void _enteruserspace(uintptr_t entry, uintptr_t user_stack_top);

uintptr_t live_elf_start = 0;
size_t live_elf_len = 0;

void userspace_init() {
#ifdef __USE_ANTARTICA__
#if defined(__x86_64__)
   if (multiboot_magic == 0x2BADB002 && multiboot_info_ptr) {
      struct multiboot_info *mbi = (struct multiboot_info *)(uintptr_t)multiboot_info_ptr;
      if ((mbi->flags & (1 << 3)) && mbi->mods_count > 0 && mbi->mods_addr) {
         struct multiboot_module *mod = (struct multiboot_module *)(uintptr_t)mbi->mods_addr;
         uintptr_t elf_start = 0;
         size_t elf_len = 0;
         uint32_t elf_string = 0;
         print_character(" [MODS:");
         print_dec(mbi->mods_count);
         print_character("] ");
         for (uint32_t m = 0; m < mbi->mods_count; m++) {
            print_character(" [M");
            print_dec(m);
            print_character(" s=");
            print_hex(mod[m].mod_start);
            print_character(" e=");
            print_hex(mod[m].mod_end);
            print_character("] ");
            uintptr_t m_start = mod[m].mod_start;
            uintptr_t m_end = mod[m].mod_end;
            if (m_end > m_start) {
               size_t m_len = m_end - m_start;
               const uint8_t *hdr = (const uint8_t *)(uintptr_t)m_start;
               if (m_len >= 4 && hdr[0] == 0x7F && hdr[1] == 'E' && hdr[2] == 'L' && hdr[3] == 'F') {
                  elf_start = m_start;
                  elf_len = m_len;
                  live_elf_start = m_start;
                  live_elf_len = m_len;
                  elf_string = mod[m].string;
               } else {
                  extern uintptr_t ext2_ramdisk_base;
                  extern size_t ext2_ramdisk_size;
                  extern void init_ext2(void);
#if defined(__x86_64__)
                  ext2_ramdisk_base = 0xFFFF800000000000ULL + m_start;
#else
                  ext2_ramdisk_base = m_start;
#endif
                  ext2_ramdisk_size = m_len;
                  init_ext2();
               }// else
            }// if
         }// for
         if (elf_start) {
            infon("Multiboot ELF module detected, executing live Linux binary");
            char *argv_list[16] = {0};
            char cmd_copy[1024];
            int argc = 0;
            const char *src = 0;
            if ((mbi->flags & (1 << 2)) && mbi->cmdline) {
               src = (const char *)(uintptr_t)mbi->cmdline;
            } else if (elf_string) {
               src = (const char *)(uintptr_t)elf_string;
            }// else if
            if (src) {
               size_t clen = 0;
               while (src[clen] && clen < sizeof(cmd_copy) - 1) {
                  cmd_copy[clen] = src[clen];
                  clen++;
               }// while
               cmd_copy[clen] = '\0';
               char *p = cmd_copy;
               while (*p && argc < 15) {
                  while (*p == ' ') {
                     *p++ = '\0';
                  }// while
                  if (!*p) {
                     break;
                  }// if
                  if (*p == '\'' || *p == '"') {
                     char quote = *p++;
                     argv_list[argc++] = p;
                     while (*p && *p != quote) {
                        p++;
                     }// while
                     if (*p == quote) {
                        *p++ = '\0';
                     }// if
                  } else {
                     argv_list[argc++] = p;
                     while (*p && *p != ' ') {
                        p++;
                     }// while
                  }// else
               }// while
               argv_list[argc] = 0;
               if (argc > 1) {
                  const char *a0 = argv_list[0];
                  size_t a0_len = strlen(a0);
                  if (a0_len >= 4 && (strcmp(a0 + a0_len - 4, ".elf") == 0 || strcmp(a0 + a0_len - 4, ".ELF") == 0 || strcmp(a0, "kernel") == 0)) {
                     for (int i = 0; i < argc - 1; i++) {
                        argv_list[i] = argv_list[i + 1];
                     }// for
                     argc--;
                     argv_list[argc] = 0;
                  }// if
               }// if
            }// if
            if (antartica_elf_exec((const void *)(uintptr_t)elf_start, elf_len, argc > 0 ? argv_list : 0, 0) == 0) {
               return;
            }// if
            errorn("Antartica ELF exec returned non-zero, falling back to default userspace");
         }// if
      }// if
   }// if
#endif
#endif
   uintptr_t user_stack_page = allocate_memory();
   uintptr_t kernel_trap_stack_page = allocate_memory();
   if (!user_stack_page || !kernel_trap_stack_page) {
      fatal("Failed to allocate initial userspace stacks");
      return;
   }// if
   uintptr_t user_stack = user_stack_page + 4096;
   extern uintptr_t *root_directory;
   if (root_directory) {
      map_virtual_memory(root_directory, user_stack_page, user_stack_page, (PTE_RW | PTE_U));
   }// if
#if defined(__RISCV__) || defined(__riscv)
   uintptr_t kernel_trap_stack = kernel_trap_stack_page + 4096;
   asm volatile("csrw mscratch, %0" :: "r"(kernel_trap_stack));
#else
   (void)kernel_trap_stack_page;
#endif
   infon("Starting and entering userspace");
   enter_userspace((uintptr_t)test_user_program, user_stack);
}// userspace_init

void enter_userspace(uintptr_t entry, uintptr_t user_stack) {
#if defined(__RISCV__) || defined(__riscv)
   uint32_t state;
   asm volatile("csrr %0, mstatus" : "=r"(state));
   state &= ~(3 << 11);
   state |= (1 << 7);
   asm volatile("csrw mstatus, %0" :: "r"(state));
   asm volatile("csrw mepc, %0" :: "r"(entry));
#else
   (void)entry;
#endif
   #ifdef __DEBUG__
      uintptr_t entry1 = (uintptr_t)test_user_program;
      create_process((void *)entry1, 0);
      uintptr_t entry2 = (uintptr_t)test_user_program;
      create_process((void *)entry2, 1);
      uintptr_t entry3 = (uintptr_t)test_user_program;
      create_process((void *)entry3, 2);
   #endif
   _enteruserspace(entry, user_stack);
}// enter_userspace
