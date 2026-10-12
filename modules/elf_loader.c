#if defined(__USE_LINUX_MODULES__)

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../include/kernel.h"
#include "../include/memory.h"
#if __has_include("../include/elf_loader.h")
#include "../include/elf_loader.h"
#else
#include "elf_loader.h"
#endif

#define MAX_LOADED_SECTIONS 32

struct section_map {
   uint64_t sh_offset;
   uint64_t sh_size;
   uintptr_t load_addr;
   uint32_t sh_type;
   uint32_t sh_flags;
};

static void free_module_allocations(struct linux_ko_module *mod) {
   if (!mod) {
      return;
   }// if
   for (size_t i = 0; i < mod->alloc_count; i++) {
      if (mod->allocs[i].addr != 0) {
         memset((void *)mod->allocs[i].addr, 0, 4096);
         free_memory((uintptr_t *)mod->allocs[i].addr);
         mod->allocs[i].addr = 0;
         mod->allocs[i].size = 0;
         mod->allocs[i].flags = 0;
      }// if
   }// for
   mod->alloc_count = 0;
   mod->init_module = NULL;
   mod->cleanup_module = NULL;
   mod->base_address = NULL;
   mod->total_size = 0;
   mod->is_loaded = false;
   mod->state = 0;
}// free_module_allocations

static const char *get_safe_string(const char *strtab, size_t strtab_size, uint64_t offset) {
   if (offset >= strtab_size) {
      return NULL;
   }// if
   for (size_t i = (size_t)offset; i < strtab_size; i++) {
      if (strtab[i] == '\0') {
         return strtab + offset;
      }// if
   }// for
   return NULL;
}// get_safe_string

#if defined(CONFIG_ELF_32)

static bool validate_elf_header_32(const Elf32_Ehdr *ehdr, size_t data_len) {
   if (data_len < sizeof(Elf32_Ehdr)) {
      errorn("elf: binary too small for ELF32 header");
      return false;
   }// if

   if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
       ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3) {
      errorn("elf: invalid magic bytes");
      return false;
   }// if

   if (ehdr->e_ident[EI_CLASS] != ELFCLASS32) {
      errorn("elf: expected 32-bit ELF class");
      return false;
   }// if

   if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
      errorn("elf: only little-endian supported");
      return false;
   }// if

   if (ehdr->e_type != ET_REL) {
      errorn("elf: object is not relocatable (ET_REL)");
      return false;
   }// if

#if defined(CONFIG_ARCH_RISCV)
   if (ehdr->e_machine != EM_RISCV) {
      errorn("elf: machine is not RISC-V");
      return false;
   }// if
#elif defined(CONFIG_ARCH_ARM)
   if (ehdr->e_machine != EM_ARM) {
      errorn("elf: machine is not ARM");
      return false;
   }// if
#elif defined(CONFIG_ARCH_X86)
   if (ehdr->e_machine != EM_386) {
      errorn("elf: machine is not x86 32-bit");
      return false;
   }// if
#else
   if (ehdr->e_machine != EM_RISCV && ehdr->e_machine != EM_ARM && ehdr->e_machine != EM_386) {
      errorn("elf: unsupported target architecture");
      return false;
   }// if
#endif

   if (ehdr->e_shentsize != sizeof(Elf32_Shdr)) {
      errorn("elf: invalid section header entry size");
      return false;
   }// if

   if ((ehdr->e_shoff & 3U) != 0) {
      errorn("elf: unaligned section header table offset");
      return false;
   }// if

   if (ehdr->e_shnum == 0 || ehdr->e_shnum > MAX_LOADED_SECTIONS) {
      errorn("elf: section count zero or exceeds maximum limit");
      return false;
   }// if

   size_t sh_tbl_size = (size_t)ehdr->e_shnum * sizeof(Elf32_Shdr);
   if (ehdr->e_shoff > data_len || sh_tbl_size > data_len - ehdr->e_shoff) {
      errorn("elf: section header table exceeds file buffer length");
      return false;
   }// if
   return true;
}// validate_elf_header_32

static int apply_relocations_32(
   const uint8_t *raw_elf,
   size_t data_len,
   const Elf32_Shdr *rel_sh,
   const struct section_map *sec_maps,
   size_t shnum,
   const Elf32_Sym *symtab,
   size_t sym_count,
   const char *strtab,
   size_t strtab_size,
   uint16_t machine
) {
#if defined(CONFIG_ARCH_RISCV)
   if (machine == EM_RISCV && rel_sh->sh_type != SHT_RELA) {
      errorn("elf: RISC-V ABI requires SHT_RELA relocations");
      return -1;
   }// if
#endif

   uint32_t target_sec_idx = rel_sh->sh_info;
   if (target_sec_idx == 0 || target_sec_idx >= shnum) {
      errorn("elf: relocation targets invalid section index");
      return -1;
   }// if

   if (sec_maps[target_sec_idx].load_addr == 0) {
      return 0;
   }// if

   uintptr_t target_base = sec_maps[target_sec_idx].load_addr;

   if ((rel_sh->sh_offset & 3U) != 0) {
      errorn("elf: unaligned relocation table offset");
      return -1;
   }// if

   if (rel_sh->sh_offset > data_len || rel_sh->sh_size > data_len - rel_sh->sh_offset) {
      errorn("elf: relocation table extends beyond buffer");
      return -1;
   }// if

   size_t entsize = rel_sh->sh_entsize;
   if (rel_sh->sh_type == SHT_RELA && entsize != sizeof(Elf32_Rela)) {
      errorn("elf: invalid SHT_RELA entry size");
      return -1;
   }// if
   if (rel_sh->sh_type == SHT_REL && entsize != sizeof(Elf32_Rel)) {
      errorn("elf: invalid SHT_REL entry size");
      return -1;
   }// if

   size_t rel_count = rel_sh->sh_size / entsize;
   const uint8_t *rel_entries = raw_elf + rel_sh->sh_offset;

   for (size_t i = 0; i < rel_count; i++) {
      uint32_t r_offset = 0;
      uint32_t r_info = 0;
      int32_t  r_addend = 0;

      if (rel_sh->sh_type == SHT_RELA) {
         const Elf32_Rela *rela = (const Elf32_Rela *)(rel_entries + (i * entsize));
         r_offset = rela->r_offset;
         r_info = rela->r_info;
         r_addend = rela->r_addend;
      } else {
         const Elf32_Rel *rel = (const Elf32_Rel *)(rel_entries + (i * entsize));
         r_offset = rel->r_offset;
         r_info = rel->r_info;
      }// if

      uint32_t sym_idx = ELF32_R_SYM(r_info);
      uint32_t rel_type = ELF32_R_TYPE(r_info);

      if (rel_type == 0) {
         continue;
      }// if

      if (sym_idx >= sym_count) {
         errorn("elf: relocation symbol index out of bounds");
         return -1;
      }// if

      const Elf32_Sym *sym = &symtab[sym_idx];
      uintptr_t sym_val = 0;

      if (sym->st_shndx == SHN_UNDEF) {
         const char *sym_name = get_safe_string(strtab, strtab_size, sym->st_name);
         if (!sym_name) {
            errorn("elf: invalid or unterminated symbol name in string table");
            return -1;
         }// if

         sym_val = (uintptr_t)linux_kmi_lookup_symbol(sym_name);
         if (sym_val == 0) {
            error("elf: undefined external symbol in Linux KMI: ");
            errorn((char *)sym_name);
            return -1;
         }// if
      } else if (sym->st_shndx < shnum && sym->st_shndx != SHN_UNDEF && sym->st_shndx < SHN_LORESERVE) {
         if (sec_maps[sym->st_shndx].load_addr == 0 || sym->st_value > sec_maps[sym->st_shndx].sh_size) {
            errorn("elf: symbol refers to unmapped section or out of bounds value");
            return -1;
         }// if
         sym_val = sec_maps[sym->st_shndx].load_addr + sym->st_value;
      } else if (sym->st_shndx == SHN_ABS) {
         sym_val = sym->st_value;
      } else {
         errorn("elf: unsupported relocation symbol section index");
         return -1;
      }// if

      if ((r_offset & 3U) != 0) {
         errorn("elf: unaligned relocation patch offset");
         return -1;
      }// if

      size_t patch_size = 4;
#if defined(CONFIG_ARCH_RISCV)
      if (machine == EM_RISCV && rel_type == R_RISCV_CALL) {
         patch_size = 8;
      }// if
#endif

      if (sec_maps[target_sec_idx].sh_size < patch_size ||
          r_offset > sec_maps[target_sec_idx].sh_size - patch_size) {
         errorn("elf: relocation target offset exceeds section bounds");
         return -1;
      }// if

      volatile uint32_t *patch_loc = (volatile uint32_t *)(target_base + r_offset);

      if (rel_sh->sh_type == SHT_REL) {
         r_addend = (int32_t)(*patch_loc);
      }// if

#if defined(CONFIG_ARCH_RISCV)
      if (machine == EM_RISCV) {
         switch (rel_type) {
            case R_RISCV_32:
               *patch_loc = (uint32_t)(sym_val + (uint32_t)r_addend);
               break;

            case R_RISCV_CALL: {
               uintptr_t pc = target_base + r_offset;
               int32_t off = (int32_t)(sym_val + r_addend - pc);
               int32_t hi = (off + 0x800) >> 12;
               int32_t lo = off - (hi << 12);
               patch_loc[0] = (patch_loc[0] & 0xFFFU) | (uint32_t)((hi & 0xFFFFFL) << 12);
               patch_loc[1] = (patch_loc[1] & 0xFFFFFU) | (uint32_t)((lo & 0xFFFL) << 20);
               break;
            }

            case R_RISCV_HI20: {
               int32_t val = (int32_t)(sym_val + r_addend);
               int32_t hi = (val + 0x800) >> 12;
               *patch_loc = (*patch_loc & 0xFFFU) | (uint32_t)((hi & 0xFFFFFL) << 12);
               break;
            }

            case R_RISCV_LO12_I: {
               int32_t val = (int32_t)(sym_val + r_addend);
               uint32_t lo = (uint32_t)(val & 0xFFFU);
               *patch_loc = (*patch_loc & 0xFFFFFU) | ((lo & 0xFFFU) << 20);
               break;
            }

            case R_RISCV_NONE:
               break;

            default:
               errorn("elf: unhandled RISC-V 32-bit relocation type");
               return -1;
         }// switch
      }
#endif

#if defined(CONFIG_ARCH_ARM)
      if (machine == EM_ARM) {
         switch (rel_type) {
            case R_ARM_NONE:
               break;

            case R_ARM_ABS32:
               *patch_loc = (uint32_t)(sym_val + (uint32_t)r_addend);
               break;

            case R_ARM_CALL:
            case R_ARM_JUMP24: {
               uintptr_t pc = target_base + r_offset;
               int32_t offset = (int32_t)(sym_val + r_addend - pc);
               if ((offset & 3) != 0 || offset < -33554432 || offset > 33554428) {
                  errorn("elf: ARM branch displacement out of 24-bit range or unaligned");
                  return -1;
               }// if
               *patch_loc = (*patch_loc & 0xFF000000U) | (((uint32_t)(offset >> 2)) & 0x00FFFFFFU);
               break;
            }

            default:
               errorn("elf: unhandled ARM 32-bit relocation type");
               return -1;
         }// switch
      }
#endif

#if defined(CONFIG_ARCH_X86)
      if (machine == EM_386) {
         switch (rel_type) {
            case R_386_NONE:
               break;

            case R_386_32:
               *patch_loc = (uint32_t)(sym_val + (uint32_t)r_addend);
               break;

            case R_386_PC32: {
               uintptr_t pc = target_base + r_offset;
               *patch_loc = (uint32_t)(sym_val + (uint32_t)r_addend - pc);
               break;
            }

            default:
               errorn("elf: unhandled x86 32-bit relocation type");
               return -1;
         }// switch
      }
#endif
   }// for

   return 0;
}// apply_relocations_32

static int load_linux_ko_32(const void *elf_data, size_t data_len, struct linux_ko_module *out_mod) {
   const uint8_t *raw = (const uint8_t *)elf_data;
   const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *)raw;

   if (!validate_elf_header_32(ehdr, data_len)) {
      return -1;
   }// if

   const Elf32_Shdr *shdrs = (const Elf32_Shdr *)(raw + ehdr->e_shoff);
   size_t shnum = ehdr->e_shnum;

   if (ehdr->e_shstrndx == 0 || ehdr->e_shstrndx >= shnum) {
      errorn("elf: invalid section header string table index");
      return -1;
   }// if

   const Elf32_Shdr *shstr_sh = &shdrs[ehdr->e_shstrndx];
   if (shstr_sh->sh_type != SHT_STRTAB) {
      errorn("elf: shstrndx section is not SHT_STRTAB");
      return -1;
   }// if

   if (shstr_sh->sh_offset > data_len || shstr_sh->sh_size > data_len - shstr_sh->sh_offset) {
      errorn("elf: section header string table out of bounds");
      return -1;
   }// if

   const char *shstrtab = (const char *)(raw + shstr_sh->sh_offset);
   if (shstr_sh->sh_size == 0 || shstrtab[0] != '\0' || shstrtab[shstr_sh->sh_size - 1] != '\0') {
      errorn("elf: section header string table not null-terminated or invalid start");
      return -1;
   }// if

   struct section_map sec_maps[MAX_LOADED_SECTIONS];
   memset(sec_maps, 0, sizeof(sec_maps));

   const Elf32_Sym *symtab = NULL;
   size_t sym_count = 0;
   uint32_t symtab_sec_idx = 0;
   uint32_t strtab_sec_idx = 0;

   size_t total_allocated_size = 0;
   void *first_loaded_base = NULL;

   /* Phase 1: Allocate memory for SHF_ALLOC sections (skipping section 0) */
   for (size_t i = 1; i < shnum; i++) {
      sec_maps[i].sh_offset = shdrs[i].sh_offset;
      sec_maps[i].sh_size = shdrs[i].sh_size;
      sec_maps[i].sh_type = shdrs[i].sh_type;
      sec_maps[i].sh_flags = (uint32_t)shdrs[i].sh_flags;
      sec_maps[i].load_addr = 0;

      if (shdrs[i].sh_type == SHT_SYMTAB) {
         if (symtab_sec_idx != 0) {
            errorn("elf: multiple symbol tables not supported");
            free_module_allocations(out_mod);
            return -1;
         }// if
         symtab_sec_idx = (uint32_t)i;
         strtab_sec_idx = shdrs[i].sh_link;
      }// if

      if ((shdrs[i].sh_flags & SHF_ALLOC) && shdrs[i].sh_size > 0) {
         if (shdrs[i].sh_size > 4096) {
            errorn("elf: section size exceeds single page capacity");
            free_module_allocations(out_mod);
            return -1;
         }// if

         if (out_mod->alloc_count >= MAX_MODULE_ALLOCATIONS) {
            errorn("elf: module exceeds maximum allocation count");
            free_module_allocations(out_mod);
            return -1;
         }// if

         uintptr_t mem = allocate_memory();
         if (mem == 0) {
            errorn("elf: out of memory allocating section");
            free_module_allocations(out_mod);
            return -1;
         }// if

         memset((void *)mem, 0, 4096);
         out_mod->allocs[out_mod->alloc_count].addr = mem;
         out_mod->allocs[out_mod->alloc_count].size = shdrs[i].sh_size;
         out_mod->allocs[out_mod->alloc_count].flags = (uint32_t)shdrs[i].sh_flags;
         out_mod->alloc_count++;

         sec_maps[i].load_addr = mem;
         total_allocated_size += shdrs[i].sh_size;
         if (!first_loaded_base) {
            first_loaded_base = (void *)mem;
         }// if

         if (shdrs[i].sh_type != SHT_NOBITS) {
            if (shdrs[i].sh_offset > data_len || shdrs[i].sh_size > data_len - shdrs[i].sh_offset) {
               errorn("elf: section content exceeds buffer boundary");
               free_module_allocations(out_mod);
               return -1;
            }// if
            memcpy((void *)mem, raw + shdrs[i].sh_offset, shdrs[i].sh_size);
         }// if
      }// if
   }// for

   /* Phase 2: Validate symbol and string tables */
   if (symtab_sec_idx == 0 || strtab_sec_idx == 0 || strtab_sec_idx >= shnum) {
      errorn("elf: missing or invalid symbol/string table");
      free_module_allocations(out_mod);
      return -1;
   }// if

   if (shdrs[strtab_sec_idx].sh_type != SHT_STRTAB) {
      errorn("elf: linked string table section is not SHT_STRTAB");
      free_module_allocations(out_mod);
      return -1;
   }// if

   const Elf32_Shdr *sym_sh = &shdrs[symtab_sec_idx];
   const Elf32_Shdr *str_sh = &shdrs[strtab_sec_idx];

   if ((sym_sh->sh_offset & 3U) != 0) {
      errorn("elf: unaligned symbol table offset");
      free_module_allocations(out_mod);
      return -1;
   }// if

   if (sym_sh->sh_offset > data_len || sym_sh->sh_size > data_len - sym_sh->sh_offset ||
       str_sh->sh_offset > data_len || str_sh->sh_size > data_len - str_sh->sh_offset) {
      errorn("elf: symbol or string table boundaries exceed buffer length");
      free_module_allocations(out_mod);
      return -1;
   }// if

   const char *strtab = (const char *)(raw + str_sh->sh_offset);
   size_t strtab_size = str_sh->sh_size;

   if (strtab_size == 0 || strtab[0] != '\0' || strtab[strtab_size - 1] != '\0') {
      errorn("elf: symbol string table not null-terminated or invalid start");
      free_module_allocations(out_mod);
      return -1;
   }// if

   if (sym_sh->sh_entsize != sizeof(Elf32_Sym) || (sym_sh->sh_size % sizeof(Elf32_Sym)) != 0) {
      errorn("elf: invalid symbol table entry size or stride");
      free_module_allocations(out_mod);
      return -1;
   }// if

   symtab = (const Elf32_Sym *)(raw + sym_sh->sh_offset);
   sym_count = sym_sh->sh_size / sizeof(Elf32_Sym);

   /* Phase 3: Apply relocations */
   for (size_t i = 1; i < shnum; i++) {
      if (shdrs[i].sh_type == SHT_REL || shdrs[i].sh_type == SHT_RELA) {
         if (shdrs[i].sh_link != symtab_sec_idx) {
            continue;
         }// if
         int res = apply_relocations_32(raw, data_len, &shdrs[i], sec_maps, shnum, symtab, sym_count, strtab, strtab_size, ehdr->e_machine);
         if (res != 0) {
            errorn("elf: relocation resolution failed");
            free_module_allocations(out_mod);
            return res;
         }// if
      }// if
   }// for

   /* Phase 4: Discover entrypoints init_module and cleanup_module */
   int (*init_fn)(void) = NULL;
   void (*clean_fn)(void) = NULL;

   for (size_t i = 0; i < sym_count; i++) {
      const char *sname = get_safe_string(strtab, strtab_size, symtab[i].st_name);
      if (sname) {
         if (strcmp(sname, "init_module") == 0) {
            uint16_t sec_idx = symtab[i].st_shndx;
            if (sec_idx != SHN_UNDEF && sec_idx < shnum && sec_idx < SHN_LORESERVE) {
               if (sec_maps[sec_idx].load_addr != 0 &&
                   (sec_maps[sec_idx].sh_flags & SHF_EXECINSTR) &&
                   symtab[i].st_value < sec_maps[sec_idx].sh_size) {
                  uintptr_t fn_addr = sec_maps[sec_idx].load_addr + symtab[i].st_value;
#if defined(CONFIG_ARCH_ARM)
                  if ((fn_addr & ~1U) != 0) {
                     init_fn = (int (*)(void))fn_addr;
                  }// if
#else
                  if ((fn_addr & 1U) == 0) {
                     init_fn = (int (*)(void))fn_addr;
                  }// if
#endif
               }// if
            }// if
         } else if (strcmp(sname, "cleanup_module") == 0) {
            uint16_t sec_idx = symtab[i].st_shndx;
            if (sec_idx != SHN_UNDEF && sec_idx < shnum && sec_idx < SHN_LORESERVE) {
               if (sec_maps[sec_idx].load_addr != 0 &&
                   (sec_maps[sec_idx].sh_flags & SHF_EXECINSTR) &&
                   symtab[i].st_value < sec_maps[sec_idx].sh_size) {
                  uintptr_t fn_addr = sec_maps[sec_idx].load_addr + symtab[i].st_value;
#if defined(CONFIG_ARCH_ARM)
                  if ((fn_addr & ~1U) != 0) {
                     clean_fn = (void (*)(void))fn_addr;
                  }// if
#else
                  if ((fn_addr & 1U) == 0) {
                     clean_fn = (void (*)(void))fn_addr;
                  }// if
#endif
               }// if
            }// if
         }// if
      }// if
   }// for

   memset(out_mod->name, 0, sizeof(out_mod->name));
   memcpy(out_mod->name, "linux_ko_module", 15);
   out_mod->base_address = first_loaded_base;
   out_mod->total_size = total_allocated_size;
   out_mod->init_module = init_fn;
   out_mod->cleanup_module = clean_fn;
   out_mod->is_loaded = true;
   out_mod->refcnt = 0;
   out_mod->state = 1;

   if (init_fn) {
      infon("elf: invoking Linux module init_module()...");
      int ret = init_fn();
      if (ret != 0) {
         errorn("elf: module init_module() returned non-zero error");
         if (clean_fn) {
            clean_fn();
         }// if
         free_module_allocations(out_mod);
         return ret;
      }// if
      successn("elf: Linux module init_module() succeeded");
   } else {
      infon("elf: no init_module symbol present; relocations complete");
   }// if

   return 0;
}// load_linux_ko_32
#endif /* CONFIG_ELF_32 */

#if defined(CONFIG_ELF_64)

static bool validate_elf_header_64(const Elf64_Ehdr *ehdr, size_t data_len) {
   if (data_len < sizeof(Elf64_Ehdr)) {
      errorn("elf: binary too small for ELF64 header");
      return false;
   }// if

   if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
       ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3) {
      errorn("elf: invalid magic bytes");
      return false;
   }// if

   if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
      errorn("elf: expected 64-bit ELF class");
      return false;
   }// if

   if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
      errorn("elf: only little-endian supported");
      return false;
   }// if

   if (ehdr->e_type != ET_REL) {
      errorn("elf: object is not relocatable (ET_REL)");
      return false;
   }// if

#if defined(CONFIG_ARCH_RISCV)
   if (ehdr->e_machine != EM_RISCV) {
      errorn("elf: machine is not RISC-V 64-bit");
      return false;
   }// if
#elif defined(CONFIG_ARCH_ARM)
   if (ehdr->e_machine != EM_AARCH64) {
      errorn("elf: machine is not AArch64");
      return false;
   }// if
#elif defined(CONFIG_ARCH_X86)
   if (ehdr->e_machine != EM_X86_64) {
      errorn("elf: machine is not x86_64");
      return false;
   }// if
#else
   if (ehdr->e_machine != EM_RISCV && ehdr->e_machine != EM_AARCH64 && ehdr->e_machine != EM_X86_64) {
      errorn("elf: unsupported 64-bit target architecture");
      return false;
   }// if
#endif

   if (ehdr->e_shentsize != sizeof(Elf64_Shdr)) {
      errorn("elf: invalid 64-bit section header entry size");
      return false;
   }// if

   if ((ehdr->e_shoff & 7U) != 0) {
      errorn("elf: unaligned 64-bit section header table offset");
      return false;
   }// if

   if (ehdr->e_shnum == 0 || ehdr->e_shnum > MAX_LOADED_SECTIONS) {
      errorn("elf: 64-bit section count zero or exceeds limit");
      return false;
   }// if

   size_t sh_tbl_size = (size_t)ehdr->e_shnum * sizeof(Elf64_Shdr);
   if (ehdr->e_shoff > data_len || sh_tbl_size > data_len - (size_t)ehdr->e_shoff) {
      errorn("elf: 64-bit section header table exceeds buffer length");
      return false;
   }// if

   return true;
}// validate_elf_header_64

static int apply_relocations_64(
   const uint8_t *raw_elf,
   size_t data_len,
   const Elf64_Shdr *rel_sh,
   const struct section_map *sec_maps,
   size_t shnum,
   const Elf64_Sym *symtab,
   size_t sym_count,
   const char *strtab,
   size_t strtab_size,
   uint16_t machine
) {
#if defined(CONFIG_ARCH_RISCV)
   if (machine == EM_RISCV && rel_sh->sh_type != SHT_RELA) {
      errorn("elf: RISC-V 64-bit ABI requires SHT_RELA relocations");
      return -1;
   }// if
#endif

   uint32_t target_sec_idx = rel_sh->sh_info;
   if (target_sec_idx == 0 || target_sec_idx >= shnum) {
      errorn("elf: 64-bit relocation targets invalid section index");
      return -1;
   }// if

   if (sec_maps[target_sec_idx].load_addr == 0) {
      return 0;
   }// if

   uintptr_t target_base = sec_maps[target_sec_idx].load_addr;

   if ((rel_sh->sh_offset & 7U) != 0) {
      errorn("elf: unaligned 64-bit relocation table offset");
      return -1;
   }// if

   if (rel_sh->sh_offset > data_len || rel_sh->sh_size > data_len - (size_t)rel_sh->sh_offset) {
      errorn("elf: 64-bit relocation table extends beyond buffer");
      return -1;
   }// if

   size_t entsize = (size_t)rel_sh->sh_entsize;
   if (rel_sh->sh_type == SHT_RELA && entsize != sizeof(Elf64_Rela)) {
      errorn("elf: invalid SHT_RELA entry size in ELF64");
      return -1;
   }// if
   if (rel_sh->sh_type == SHT_REL && entsize != sizeof(Elf64_Rel)) {
      errorn("elf: invalid SHT_REL entry size in ELF64");
      return -1;
   }// if

   size_t rel_count = (size_t)(rel_sh->sh_size / entsize);
   const uint8_t *rel_entries = raw_elf + rel_sh->sh_offset;

   for (size_t i = 0; i < rel_count; i++) {
      uint64_t r_offset = 0;
      uint64_t r_info = 0;
      int64_t r_addend = 0;

      if (rel_sh->sh_type == SHT_RELA) {
         const Elf64_Rela *rela = (const Elf64_Rela *)(rel_entries + (i * entsize));
         r_offset = rela->r_offset;
         r_info = rela->r_info;
         r_addend = rela->r_addend;
      } else {
         const Elf64_Rel *rel = (const Elf64_Rel *)(rel_entries + (i * entsize));
         r_offset = rel->r_offset;
         r_info = rel->r_info;
         r_addend = 0;
      }// if

      uint32_t sym_idx = (uint32_t)ELF64_R_SYM(r_info);
      uint32_t rel_type = ELF64_R_TYPE(r_info);

      if (rel_type == 0) {
         continue;
      }// if

      if (sym_idx >= sym_count) {
         errorn("elf: relocation symbol index out of bounds");
         return -1;
      }// if

      const Elf64_Sym *sym = &symtab[sym_idx];
      uintptr_t sym_val = 0;

      if (sym->st_shndx == SHN_UNDEF) {
         const char *sym_name = get_safe_string(strtab, strtab_size, sym->st_name);
         if (!sym_name) {
            errorn("elf: invalid symbol name in string table");
            return -1;
         }// if

         sym_val = (uintptr_t)linux_kmi_lookup_symbol(sym_name);
         if (sym_val == 0) {
            error("elf: undefined external symbol in Linux KMI: ");
            errorn((char *)sym_name);
            return -1;
         }// if
      } else if (sym->st_shndx < shnum && sym->st_shndx != SHN_UNDEF && sym->st_shndx < SHN_LORESERVE) {
         if (sec_maps[sym->st_shndx].load_addr == 0 || sym->st_value > sec_maps[sym->st_shndx].sh_size) {
            errorn("elf: symbol refers to unmapped section or out of bounds value");
            return -1;
         }// if
         sym_val = sec_maps[sym->st_shndx].load_addr + (uintptr_t)sym->st_value;
      } else if (sym->st_shndx == SHN_ABS) {
         sym_val = (uintptr_t)sym->st_value;
      } else {
         errorn("elf: unsupported relocation symbol section index");
         return -1;
      }// if

      size_t patch_size = 4;
#if defined(CONFIG_ARCH_RISCV)
      if (machine == EM_RISCV && (rel_type == R_RISCV_64 || rel_type == R_RISCV_CALL)) {
         patch_size = 8;
      }// if
#endif
#if defined(CONFIG_ARCH_ARM)
      if (machine == EM_AARCH64 && rel_type == R_AARCH64_ABS64) {
         patch_size = 8;
      }// if
#endif
#if defined(CONFIG_ARCH_X86)
      if (machine == EM_X86_64 && rel_type == R_X86_64_64) {
         patch_size = 8;
      }// if
#endif

      if ((r_offset & 3U) != 0 || (patch_size == 8 && (r_offset & 7U) != 0)) {
         errorn("elf: unaligned 64-bit relocation target offset");
         return -1;
      }// if

      if (sec_maps[target_sec_idx].sh_size < patch_size ||
          r_offset > sec_maps[target_sec_idx].sh_size - patch_size) {
         errorn("elf: 64-bit relocation target offset exceeds section bounds");
         return -1;
      }// if

      volatile uint32_t *patch_loc32 = (volatile uint32_t *)(target_base + r_offset);
      volatile uint64_t *patch_loc64 = (volatile uint64_t *)(target_base + r_offset);

      if (rel_sh->sh_type == SHT_REL) {
         if (patch_size == 8) {
            r_addend = (int64_t)(*patch_loc64);
         } else {
            r_addend = (int64_t)(int32_t)(*patch_loc32);
         }// if
      }// if

#if defined(CONFIG_ARCH_RISCV)
      if (machine == EM_RISCV) {
         switch (rel_type) {
            case R_RISCV_64:
               *patch_loc64 = (uint64_t)(sym_val + (uint64_t)r_addend);
               break;

            case R_RISCV_32:
               *patch_loc32 = (uint32_t)(sym_val + (uint32_t)r_addend);
               break;

            case R_RISCV_CALL: {
               uintptr_t pc = target_base + (uintptr_t)r_offset;
               int64_t off = (int64_t)(sym_val + r_addend - pc);
               int64_t hi = (off + 0x800LL) >> 12;
               int64_t lo = off - (hi << 12);
               patch_loc32[0] = (patch_loc32[0] & 0xFFFU) | (uint32_t)((hi & 0xFFFFFL) << 12);
               patch_loc32[1] = (patch_loc32[1] & 0xFFFFFU) | (uint32_t)((lo & 0xFFFL) << 20);
               break;
            }

            case R_RISCV_HI20: {
               int64_t val = (int64_t)(sym_val + r_addend);
               int64_t hi = (val + 0x800LL) >> 12;
               *patch_loc32 = (*patch_loc32 & 0xFFFU) | (uint32_t)((hi & 0xFFFFFL) << 12);
               break;
            }

            case R_RISCV_LO12_I: {
               int64_t val = (int64_t)(sym_val + r_addend);
               uint32_t lo = (uint32_t)(val & 0xFFFU);
               *patch_loc32 = (*patch_loc32 & 0xFFFFFU) | ((lo & 0xFFFU) << 20);
               break;
            }

            case R_RISCV_NONE:
               break;

            default:
               errorn("elf: unhandled RISC-V 64-bit relocation type");
               return -1;
         }// switch
      }
#endif

#if defined(CONFIG_ARCH_ARM)
      if (machine == EM_AARCH64) {
         switch (rel_type) {
            case R_AARCH64_NONE:
               break;

            case R_AARCH64_ABS64:
               *patch_loc64 = (uint64_t)(sym_val + (uint64_t)r_addend);
               break;

            case R_AARCH64_ABS32:
               *patch_loc32 = (uint32_t)(sym_val + (uint32_t)r_addend);
               break;

            case R_AARCH64_CALL26: {
               uintptr_t pc = target_base + (uintptr_t)r_offset;
               int64_t offset = (int64_t)(sym_val + r_addend - pc);
               if ((offset & 3LL) != 0 || offset < -134217728LL || offset > 134217724LL) {
                  errorn("elf: AArch64 CALL26 displacement out of branch range or unaligned");
                  return -1;
               }// if
               *patch_loc32 = (*patch_loc32 & 0xFC000000U) | (((uint32_t)(offset >> 2)) & 0x03FFFFFFU);
               break;
            }

            default:
               errorn("elf: unhandled AArch64 relocation type");
               return -1;
         }// switch
      }
#endif

#if defined(CONFIG_ARCH_X86)
      if (machine == EM_X86_64) {
         switch (rel_type) {
            case R_X86_64_NONE:
               break;

            case R_X86_64_64:
               *patch_loc64 = (uint64_t)(sym_val + (uint64_t)r_addend);
               break;

            case R_X86_64_32: {
               uint64_t val = (uint64_t)(sym_val + r_addend);
               if (val > 0xFFFFFFFFULL) {
                  errorn("elf: x86_64 R_X86_64_32 value exceeds 32-bit range");
                  return -1;
               }// if
               *patch_loc32 = (uint32_t)val;
               break;
            }

            case R_X86_64_PC32: {
               uintptr_t pc = target_base + (uintptr_t)r_offset;
               int64_t disp = (int64_t)(sym_val + r_addend - pc);
               if (disp < -0x80000000LL || disp > 0x7FFFFFFFLL) {
                  errorn("elf: x86_64 PC32 displacement out of 32-bit range");
                  return -1;
               }// if
               *patch_loc32 = (uint32_t)disp;
               break;
            }

            default:
               errorn("elf: unhandled x86_64 relocation type");
               return -1;
         }// switch
      }
#endif
   }// for

   return 0;
}// apply_relocations_64

static int load_linux_ko_64(const void *elf_data, size_t data_len, struct linux_ko_module *out_mod) {
   const uint8_t *raw = (const uint8_t *)elf_data;
   const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)raw;

   if (!validate_elf_header_64(ehdr, data_len)) {
      return -1;
   }// if

   const Elf64_Shdr *shdrs = (const Elf64_Shdr *)(raw + (size_t)ehdr->e_shoff);
   size_t shnum = (size_t)ehdr->e_shnum;

   if (ehdr->e_shstrndx == 0 || ehdr->e_shstrndx >= shnum) {
      errorn("elf: invalid section header string table index");
      return -1;
   }// if

   const Elf64_Shdr *shstr_sh = &shdrs[ehdr->e_shstrndx];
   if (shstr_sh->sh_type != SHT_STRTAB) {
      errorn("elf: shstrndx section is not SHT_STRTAB");
      return -1;
   }// if

   if (shstr_sh->sh_offset > data_len || shstr_sh->sh_size > data_len - (size_t)shstr_sh->sh_offset) {
      errorn("elf: section header string table out of bounds");
      return -1;
   }// if

   const char *shstrtab = (const char *)(raw + (size_t)shstr_sh->sh_offset);
   if (shstr_sh->sh_size == 0 || shstrtab[0] != '\0' || shstrtab[shstr_sh->sh_size - 1] != '\0') {
      errorn("elf: section header string table not null-terminated or invalid start");
      return -1;
   }// if

   struct section_map sec_maps[MAX_LOADED_SECTIONS];
   memset(sec_maps, 0, sizeof(sec_maps));

   const Elf64_Sym *symtab = NULL;
   size_t sym_count = 0;
   uint32_t symtab_sec_idx = 0;
   uint32_t strtab_sec_idx = 0;
   size_t total_allocated_size = 0;
   void *first_loaded_base = NULL;
   for (size_t i = 1; i < shnum; i++) {
      sec_maps[i].sh_offset = shdrs[i].sh_offset;
      sec_maps[i].sh_size = shdrs[i].sh_size;
      sec_maps[i].sh_type = shdrs[i].sh_type;
      sec_maps[i].sh_flags = (uint32_t)shdrs[i].sh_flags;
      sec_maps[i].load_addr = 0;

      if (shdrs[i].sh_type == SHT_SYMTAB) {
         if (symtab_sec_idx != 0) {
            errorn("elf: multiple symbol tables not supported");
            free_module_allocations(out_mod);
            return -1;
         }// if
         symtab_sec_idx = (uint32_t)i;
         strtab_sec_idx = (uint32_t)shdrs[i].sh_link;
      }// if

      if ((shdrs[i].sh_flags & SHF_ALLOC) && shdrs[i].sh_size > 0) {
         if (shdrs[i].sh_size > 4096) {
            errorn("elf: section size exceeds single page capacity");
            free_module_allocations(out_mod);
            return -1;
         }// if

         if (out_mod->alloc_count >= MAX_MODULE_ALLOCATIONS) {
            errorn("elf: module exceeds maximum allocation count");
            free_module_allocations(out_mod);
            return -1;
         }// if

         uintptr_t mem = allocate_memory();
         if (mem == 0) {
            errorn("elf: out of memory allocating section");
            free_module_allocations(out_mod);
            return -1;
         }// if

         memset((void *)mem, 0, 4096);
         out_mod->allocs[out_mod->alloc_count].addr = mem;
         out_mod->allocs[out_mod->alloc_count].size = (size_t)shdrs[i].sh_size;
         out_mod->allocs[out_mod->alloc_count].flags = (uint32_t)shdrs[i].sh_flags;
         out_mod->alloc_count++;

         sec_maps[i].load_addr = mem;
         total_allocated_size += (size_t)shdrs[i].sh_size;
         if (!first_loaded_base) {
            first_loaded_base = (void *)mem;
         }// if

         if (shdrs[i].sh_type != SHT_NOBITS) {
            if (shdrs[i].sh_offset > data_len || shdrs[i].sh_size > data_len - (size_t)shdrs[i].sh_offset) {
               errorn("elf: section content exceeds buffer boundary");
               free_module_allocations(out_mod);
               return -1;
            }// if
            memcpy((void *)mem, raw + (size_t)shdrs[i].sh_offset, (size_t)shdrs[i].sh_size);
         }// if
      }// if
   }// for

   if (symtab_sec_idx == 0 || strtab_sec_idx == 0 || strtab_sec_idx >= shnum) {
      errorn("elf: missing or invalid symbol/string table");
      free_module_allocations(out_mod);
      return -1;
   }// if

   if (shdrs[strtab_sec_idx].sh_type != SHT_STRTAB) {
      errorn("elf: linked string table section is not SHT_STRTAB");
      free_module_allocations(out_mod);
      return -1;
   }// if

   const Elf64_Shdr *sym_sh = &shdrs[symtab_sec_idx];
   const Elf64_Shdr *str_sh = &shdrs[strtab_sec_idx];

   if ((sym_sh->sh_offset & 7U) != 0) {
      errorn("elf: unaligned symbol table offset");
      free_module_allocations(out_mod);
      return -1;
   }// if

   if (sym_sh->sh_offset > data_len || sym_sh->sh_size > data_len - (size_t)sym_sh->sh_offset ||
       str_sh->sh_offset > data_len || str_sh->sh_size > data_len - (size_t)str_sh->sh_offset) {
      errorn("elf: symbol or string table boundaries exceed buffer length");
      free_module_allocations(out_mod);
      return -1;
   }// if

   const char *strtab = (const char *)(raw + (size_t)str_sh->sh_offset);
   size_t strtab_size = (size_t)str_sh->sh_size;

   if (strtab_size == 0 || strtab[0] != '\0' || strtab[strtab_size - 1] != '\0') {
      errorn("elf: symbol string table not null-terminated or invalid start");
      free_module_allocations(out_mod);
      return -1;
   }// if

   if (sym_sh->sh_entsize != sizeof(Elf64_Sym) || (sym_sh->sh_size % sizeof(Elf64_Sym)) != 0) {
      errorn("elf: invalid 64-bit symbol table entry size or stride");
      free_module_allocations(out_mod);
      return -1;
   }// if

   symtab = (const Elf64_Sym *)(raw + (size_t)sym_sh->sh_offset);
   sym_count = (size_t)(sym_sh->sh_size / sizeof(Elf64_Sym));

   for (size_t i = 1; i < shnum; i++) {
      if (shdrs[i].sh_type == SHT_REL || shdrs[i].sh_type == SHT_RELA) {
         if ((uint32_t)shdrs[i].sh_link != symtab_sec_idx) {
            continue;
         }// if
         int res = apply_relocations_64(raw, data_len, &shdrs[i], sec_maps, shnum, symtab, sym_count, strtab, strtab_size, ehdr->e_machine);
         if (res != 0) {
            errorn("elf: relocation resolution failed");
            free_module_allocations(out_mod);
            return res;
         }// if
      }// if
   }// for

   int (*init_fn)(void) = NULL;
   void (*clean_fn)(void) = NULL;

   for (size_t i = 0; i < sym_count; i++) {
      const char *sname = get_safe_string(strtab, strtab_size, symtab[i].st_name);
      if (sname) {
         if (strcmp(sname, "init_module") == 0) {
            uint16_t sec_idx = symtab[i].st_shndx;
            if (sec_idx != SHN_UNDEF && sec_idx < shnum && sec_idx < SHN_LORESERVE) {
               if (sec_maps[sec_idx].load_addr != 0 &&
                   (sec_maps[sec_idx].sh_flags & SHF_EXECINSTR) &&
                   symtab[i].st_value < sec_maps[sec_idx].sh_size) {
                  uintptr_t fn_addr = sec_maps[sec_idx].load_addr + (uintptr_t)symtab[i].st_value;
                  if ((fn_addr & 1U) == 0) {
                     init_fn = (int (*)(void))fn_addr;
                  }// if
               }// if
            }// if
         } else if (strcmp(sname, "cleanup_module") == 0) {
            uint16_t sec_idx = symtab[i].st_shndx;
            if (sec_idx != SHN_UNDEF && sec_idx < shnum && sec_idx < SHN_LORESERVE) {
               if (sec_maps[sec_idx].load_addr != 0 &&
                   (sec_maps[sec_idx].sh_flags & SHF_EXECINSTR) &&
                   symtab[i].st_value < sec_maps[sec_idx].sh_size) {
                  uintptr_t fn_addr = sec_maps[sec_idx].load_addr + (uintptr_t)symtab[i].st_value;
                  if ((fn_addr & 1U) == 0) {
                     clean_fn = (void (*)(void))fn_addr;
                  }// if
               }// if
            }// if
         }// if
      }// if
   }// for

   memset(out_mod->name, 0, sizeof(out_mod->name));
   memcpy(out_mod->name, "linux_ko_module", 15);
   out_mod->base_address = first_loaded_base;
   out_mod->total_size = total_allocated_size;
   out_mod->init_module = init_fn;
   out_mod->cleanup_module = clean_fn;
   out_mod->is_loaded = true;
   out_mod->refcnt = 0;
   out_mod->state = 1;

   if (init_fn) {
      infon("elf: invoking Linux module init_module()...");
      int ret = init_fn();
      if (ret != 0) {
         errorn("elf: module init_module() returned non-zero error");
         if (clean_fn) {
            clean_fn();
         }// if
         free_module_allocations(out_mod);
         return ret;
      }// if
      successn("elf: Linux module init_module() succeeded");
   } else {
      infon("elf: no init_module symbol present; relocations complete");
   }// if

   return 0;
}// load_linux_ko_64
#endif
int load_linux_ko(const void *elf_data, size_t data_len, struct linux_ko_module *out_mod) {
   if (!elf_data || !out_mod) {
      errorn("elf: NULL pointer passed to load_linux_ko");
      return -1;
   }// if

   if (data_len < EI_NIDENT) {
      errorn("elf: payload too small for ELF ident");
      return -1;
   }// if

   memset(out_mod, 0, sizeof(struct linux_ko_module));
   const uint8_t *raw = (const uint8_t *)elf_data;

#if defined(CONFIG_ELF_32)
   if (raw[EI_CLASS] == ELFCLASS32) {
      return load_linux_ko_32(elf_data, data_len, out_mod);
   }// if
#endif

#if defined(CONFIG_ELF_64)
   if (raw[EI_CLASS] == ELFCLASS64) {
      return load_linux_ko_64(elf_data, data_len, out_mod);
   }// if
#endif

   errorn("elf: unsupported or unconfigured ELF class bitness");
   return -1;
}// load_linux_ko

int unload_linux_ko(struct linux_ko_module *mod) {
   if (!mod || !mod->is_loaded) {
      return -1;
   }// if

   if (__atomic_load_n(&mod->refcnt, __ATOMIC_ACQUIRE) > 0) {
      errorn("elf: cannot unload module; active reference count");
      return -16;
   }// if

   if (mod->cleanup_module) {
      infon("elf: invoking Linux module cleanup_module()...");
      mod->cleanup_module();
   }// if

   free_module_allocations(mod);
   successn("elf: Linux module unloaded and memory freed");
   return 0;
}// unload_linux_ko

#endif
