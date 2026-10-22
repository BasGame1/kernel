#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "../include/memory.h"
#include "../include/elf_loader.h"
#include "../include/kernel.h"
#include "../include/antartica.h"

typedef struct {
   uint64_t a_type;
   uint64_t a_val;
} Elf64_auxv_t;

extern int ext2_read(const char *path, void *buffer, uint32_t count, uint32_t offset);

#define AT_NULL 0
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_PAGESZ 6
#define AT_BASE 7
#define AT_FLAGS 8
#define AT_ENTRY 9
#define AT_UID 11
#define AT_EUID 12
#define AT_GID 13
#define AT_EGID 14
#define AT_CLKTCK 17
#define AT_SECURE 23
#define AT_RANDOM 25
#define AT_EXECFN 31

static inline bool is_user_elf_range(uint64_t vaddr, uint64_t memsz) {
   if (vaddr < 0x10000ULL || memsz == 0 || vaddr + memsz <= vaddr) {
      return false;
   }// if
#if defined(__x86_64__)
   if (vaddr < 0x200000ULL || vaddr + memsz > 0x0000800000000000ULL) {
      return false;
   }// if
#elif defined(__64BIT__)
   if (vaddr + memsz > 0x0000800000000000ULL) {
      return false;
   }// if
   uint64_t end = vaddr + memsz;
   if (vaddr < END_MEMORY && end > START_MEMORY) {
      return false;
   }// if
#else
   if (vaddr + memsz > 0x80000000ULL || memsz > 0xFFFFFFFFULL) {
      return false;
   }// if
   uint64_t end = vaddr + memsz;
   if (vaddr < END_MEMORY && end > START_MEMORY) {
      return false;
   }// if
#endif
   return true;
}// is_user_elf_range

static int map_user_range(uintptr_t vaddr, size_t size, uint32_t flags) {
   if (vaddr + size < vaddr) {
      return -1;
   }// if
   uintptr_t start = vaddr & ~(PAGE - 1);
   uintptr_t end = (vaddr + size + PAGE - 1) & ~(PAGE - 1);
   if (end < start) {
      return -1;
   }// if
   for (uintptr_t va = start; va < end; va += PAGE) {
      uintptr_t pa = allocate_memory();
      if (!pa) {
         return -1;
      }// if
      memset((void *)pa, 0, PAGE);
      map_virtual_memory(root_directory, va, pa, flags | PTE_V | PTE_U);
   }// for
   return 0;
}// map_user_range

uint32_t antartica_elf_exec(const void *elf_bytes, size_t elf_len, char **argv, char **envp) {
   (void)envp;
   if (!elf_bytes || ((uintptr_t)elf_bytes & 3) != 0 || elf_len < sizeof(Elf64_Ehdr)) {
      return (uint32_t)-1;
   }// if
   Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf_bytes;
   if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' || ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
      return (uint32_t)-1;
   }// if
   if (ehdr->e_ident[4] != 2 || ehdr->e_phentsize != sizeof(Elf64_Phdr)) {
      return (uint32_t)-1;
   }// if
   if (ehdr->e_phnum == 0 || ehdr->e_phnum > 64) {
      return (uint32_t)-1;
   }// if
   uint64_t phdr_size = (uint64_t)ehdr->e_phnum * sizeof(Elf64_Phdr);
   if (ehdr->e_phoff > elf_len || phdr_size > elf_len || ehdr->e_phoff + phdr_size > elf_len) {
      return (uint32_t)-1;
   }// if
   uint64_t load_bias = (ehdr->e_type == 3) ? 0x0000555555554000ULL : 0;
   uint64_t main_entry = load_bias + ehdr->e_entry;
   if (!is_user_elf_range(main_entry, 1)) {
      return (uint32_t)-1;
   }// if

   Elf64_Phdr *phdrs = (Elf64_Phdr *)((uintptr_t)elf_bytes + (uintptr_t)ehdr->e_phoff);
   uint64_t phdr_vaddr = 0;
#if defined(__x86_64__) || defined(__64BIT__)
   uint64_t stack_top = 0x7FFFFFFFE000ULL;
#else
   uint64_t stack_top = 0x7FFFE000UL;
#endif
   uint64_t stack_base = stack_top - 0x200000;
   const char *interp_path = NULL;

   for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
      if (phdrs[i].p_type == 3) {
         if (phdrs[i].p_offset < elf_len && phdrs[i].p_filesz > 0 && phdrs[i].p_offset + phdrs[i].p_filesz <= elf_len) {
            interp_path = (const char *)((uintptr_t)elf_bytes + (uintptr_t)phdrs[i].p_offset);
         }// if
      }// if
      if (phdrs[i].p_type == PT_LOAD) {
         if (phdrs[i].p_filesz > phdrs[i].p_memsz) {
            return (uint32_t)-1;
         }// if
         if (phdrs[i].p_offset > elf_len || phdrs[i].p_filesz > elf_len || phdrs[i].p_offset + phdrs[i].p_filesz > elf_len) {
            return (uint32_t)-1;
         }// if
         uint64_t seg_va = load_bias + phdrs[i].p_vaddr;
         if (!is_user_elf_range(seg_va, phdrs[i].p_memsz)) {
            return (uint32_t)-1;
         }// if
         uint64_t page_offset = phdrs[i].p_vaddr & (PAGE - 1);
         if (phdrs[i].p_offset < page_offset) {
            return (uint32_t)-1;
         }// if
         uint64_t mem_start = seg_va - page_offset;
         uint64_t file_start = phdrs[i].p_offset - page_offset;
         uint64_t file_len = phdrs[i].p_filesz + page_offset;
         uint64_t mem_len = phdrs[i].p_memsz + page_offset;
         uint64_t total_map = (mem_len + PAGE - 1) & ~(PAGE - 1);
         if (mem_start < stack_top && mem_start + total_map > stack_base) {
            return (uint32_t)-1;
         }// if
         uint32_t flags = PTE_U | PTE_R | PTE_W;
         if (map_user_range((uintptr_t)mem_start, (size_t)total_map, flags) != 0) {
            return (uint32_t)-1;
         }// if
         memcpy((void *)(uintptr_t)mem_start, (const void *)((const uint8_t *)elf_bytes + (size_t)file_start), (size_t)file_len);
         if (total_map > file_len) {
            memset((void *)(uintptr_t)(mem_start + file_len), 0, (size_t)(total_map - file_len));
         }// if
      } else if (phdrs[i].p_type == PT_PHDR) {
         phdr_vaddr = load_bias + phdrs[i].p_vaddr;
      }// if
   }// for
   if (phdr_vaddr == 0 && ehdr->e_phnum > 0) {
      phdr_vaddr = load_bias + phdrs[0].p_vaddr + ehdr->e_phoff;
   }// if

   uint64_t entry_point = main_entry;
   uint64_t at_base = 0;
   if (interp_path) {
      Elf64_Ehdr in_ehdr;
      if (ext2_read(interp_path, &in_ehdr, sizeof(Elf64_Ehdr), 0) == sizeof(Elf64_Ehdr)) {
         if (in_ehdr.e_ident[0] == 0x7F && in_ehdr.e_ident[1] == 'E' && in_ehdr.e_ident[2] == 'L' && in_ehdr.e_ident[3] == 'F' && in_ehdr.e_ident[4] == 2 && in_ehdr.e_phentsize == sizeof(Elf64_Phdr) && in_ehdr.e_phnum > 0 && in_ehdr.e_phnum <= 32) {
            Elf64_Phdr in_phdrs[32];
            size_t in_ph_size = (size_t)in_ehdr.e_phnum * sizeof(Elf64_Phdr);
            if (ext2_read(interp_path, in_phdrs, (uint32_t)in_ph_size, (uint32_t)in_ehdr.e_phoff) == (int)in_ph_size) {
               uint64_t interp_base = (in_ehdr.e_type == 3) ? 0x700000000000ULL : 0;
               for (uint16_t j = 0; j < in_ehdr.e_phnum; j++) {
                  if (in_phdrs[j].p_type == PT_LOAD) {
                     if (in_phdrs[j].p_filesz > in_phdrs[j].p_memsz) {
                        return (uint32_t)-1;
                     }// if
                     uint64_t in_seg_va = interp_base + in_phdrs[j].p_vaddr;
                     if (!is_user_elf_range(in_seg_va, in_phdrs[j].p_memsz)) {
                        return (uint32_t)-1;
                     }// if
                     uint64_t in_page_off = in_phdrs[j].p_vaddr & (PAGE - 1);
                     if (in_phdrs[j].p_offset < in_page_off) {
                        return (uint32_t)-1;
                     }// if
                     uint64_t in_mem_start = in_seg_va - in_page_off;
                     uint64_t in_file_start = in_phdrs[j].p_offset - in_page_off;
                     uint64_t in_file_len = in_phdrs[j].p_filesz + in_page_off;
                     uint64_t in_mem_len = in_phdrs[j].p_memsz + in_page_off;
                     uint64_t in_total_map = (in_mem_len + PAGE - 1) & ~(PAGE - 1);
                     if (map_user_range((uintptr_t)in_mem_start, (size_t)in_total_map, PTE_RW) == 0) {
                        if (in_file_len > 0) {
                           ext2_read(interp_path, (void *)(uintptr_t)in_mem_start, (uint32_t)in_file_len, (uint32_t)in_file_start);
                        }// if
                        if (in_total_map > in_file_len) {
                           memset((void *)(uintptr_t)(in_mem_start + in_file_len), 0, (size_t)(in_total_map - in_file_len));
                        }// if
                     }// if
                  }// if
               }// for
               entry_point = interp_base + in_ehdr.e_entry;
               at_base = interp_base;
            }// if
         }// if
      }// if
   }// if

   if (map_user_range((uintptr_t)stack_base, 0x200000, PTE_RW) != 0) {
      return (uint32_t)-1;
   }// if
   char *str_top = (char *)(uintptr_t)stack_top;
   int argc = 0;
   uint64_t argv_ptrs[16];
   if (!argv || !argv[0]) {
      str_top -= 16;
      memcpy(str_top, "busybox", 8);
      argv_ptrs[0] = (uint64_t)(uintptr_t)str_top;
      argc = 1;
   } else {
      for (int i = 0; argv[i] != NULL && i < 15; i++) {
         size_t slen = strlen(argv[i]) + 1;
         size_t aligned_len = (slen + 15) & ~15;
         if ((uintptr_t)str_top - aligned_len < (uintptr_t)stack_base + 4096) {
            break;
         }// if
         str_top -= aligned_len;
         memcpy(str_top, argv[i], slen);
         argv_ptrs[i] = (uint64_t)(uintptr_t)str_top;
         argc++;
      }// for
   }// else
   uint64_t *sp = (uint64_t *)((uintptr_t)str_top & ~(uintptr_t)0xF);
   sp -= 2;
   uint64_t rand_seed = 0x5EEDFEEDBAADF00DULL ^ (uint64_t)(uintptr_t)main_entry;
   sp[0] = rand_seed;
   sp[1] = rand_seed ^ 0x1234567890ABCDEFULL;
   uint64_t rand_addr = (uint64_t)(uintptr_t)sp;
   sp = (uint64_t *)((uintptr_t)sp & ~(uintptr_t)0xF);
   Elf64_auxv_t auxv[] = {
      { AT_PAGESZ, PAGE },
      { AT_PHDR, phdr_vaddr },
      { AT_PHENT, sizeof(Elf64_Phdr) },
      { AT_PHNUM, ehdr->e_phnum },
      { AT_BASE, at_base },
      { AT_FLAGS, 0 },
      { AT_ENTRY, main_entry },
      { AT_UID, 0 },
      { AT_EUID, 0 },
      { AT_GID, 0 },
      { AT_EGID, 0 },
      { AT_CLKTCK, 100 },
      { AT_SECURE, 0 },
      { AT_RANDOM, rand_addr },
      { AT_EXECFN, argv_ptrs[0] },
      { AT_NULL, 0 }
   };
   size_t auxv_words = sizeof(auxv) / (sizeof(uint64_t));
   if (((argc + 3) & 1) != 0) {
      sp--;
   }// if
   sp -= auxv_words;
   memcpy(sp, auxv, sizeof(auxv));
   sp -= (argc + 3);
   sp[0] = (uint64_t)argc;
   for (int i = 0; i < argc; i++) {
      sp[1 + i] = argv_ptrs[i];
   }// for
   sp[1 + argc] = 0;
   sp[2 + argc] = 0;
   _enteruserspace((uintptr_t)entry_point, (uintptr_t)sp);
   return 0;
}// antartica_elf_exec
