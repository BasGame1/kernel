#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>

#include "../include/kernel.h"
#include "../include/gid.h"
#include "../include/memory.h"
#include "../include/hardware.h"

#if defined(__USE_LINUX_MODULES__)

#if __has_include("../include/elf_loader.h")
#include "../include/elf_loader.h"
#else
#include "elf_loader.h"
#endif

#define FAST_PAGE_POOL_CAPACITY 8

extern int _end;
extern uint32_t pkcall_map_device(uintptr_t phys_addr, uintptr_t virt_addr);

struct kmi_symbol {
   const char *name;
   void *addr;
};

struct linux_spinlock {
   volatile uint8_t lock;
};

static void linux_spin_lock_init(struct linux_spinlock *sl) {
   if (sl) {
      sl->lock = 0;
   }// if
}// linux_spin_lock_init

static void linux_spin_lock(struct linux_spinlock *sl) {
   if (!sl) {
      fataln("kmi: attempt to acquire NULL spinlock");
      return;
   }// if
   while (__atomic_test_and_set(&sl->lock, __ATOMIC_ACQUIRE)) {
      /* Spin wait */
   }// while
}// linux_spin_lock

static void linux_spin_unlock(struct linux_spinlock *sl) {
   if (!sl) {
      errorn("kmi: attempt to release NULL spinlock");
      return;
   }// if
   __atomic_clear(&sl->lock, __ATOMIC_RELEASE);
}// linux_spin_unlock


static uintptr_t s_fast_page_cache[FAST_PAGE_POOL_CAPACITY];
static size_t s_fast_cache_count = 0;
static struct linux_spinlock s_cache_lock = { 0 };

void linux_kmi_flush_cache(void) {
   linux_spin_lock(&s_cache_lock);
   while (s_fast_cache_count > 0) {
      uintptr_t page = s_fast_page_cache[--s_fast_cache_count];
      free_memory((uintptr_t *)page);
   }// while
   linux_spin_unlock(&s_cache_lock);
}// linux_kmi_flush_cache

static void *linux_kmalloc(size_t size, int flags) {
   (void)flags;
   if (size == 0 || size > 4096) {
      errorn("kmi: kmalloc size 0 or exceeds single page capacity");
      return NULL;
   }// if

   uintptr_t page = 0;

   linux_spin_lock(&s_cache_lock);
   if (s_fast_cache_count > 0) {
      page = s_fast_page_cache[--s_fast_cache_count];
   }// if
   linux_spin_unlock(&s_cache_lock);

   if (page == 0) {
      page = allocate_memory();
   }// if

   if (page == 0) {
      errorn("kmi: kmalloc out of memory");
      return NULL;
   }// if

   memset((void *)page, 0, 4096);
   return (void *)page;
}// linux_kmalloc

static void *linux_kzalloc(size_t size, int flags) {
   void *ptr = linux_kmalloc(size, flags);
   if (ptr) {
      memset(ptr, 0, size);
   }// if
   return ptr;
}// linux_kzalloc

static void linux_kfree(const void *ptr) {
   if (!ptr) {
      return;
   }// if

   uintptr_t addr = (uintptr_t)ptr;
   uintptr_t min_heap_addr = ((uintptr_t)&_end + 4095U) & ~0xFFFU;

   if ((addr & 0xFFFU) != 0 || addr < min_heap_addr || addr >= END_MEMORY) {
      errorn("kmi: kfree invalid non-page or out-of-range heap pointer");
      return;
   }// if

   memset((void *)addr, 0, 4096);

   bool cached = false;
   linux_spin_lock(&s_cache_lock);
   if (s_fast_cache_count < FAST_PAGE_POOL_CAPACITY) {
      s_fast_page_cache[s_fast_cache_count++] = addr;
      cached = true;
   }// if
   linux_spin_unlock(&s_cache_lock);

   if (!cached) {
      free_memory((uintptr_t *)ptr);
   }// if
}// linux_kfree

static int linux_printk(const char *fmt, ...) {
   if (!fmt) {
      return -1;
   }// if
   char buffer[256];
   size_t bidx = 0;
   va_list args;
   va_start(args, fmt);

   for (size_t i = 0; fmt[i] != '\0' && bidx < sizeof(buffer) - 1; i++) {
      if (fmt[i] == '%' && fmt[i + 1] != '\0') {
         i++;
         if (fmt[i] == 's') {
            const char *str = va_arg(args, const char *);
            if (!str) {
               str = "(null)";
            }// if
            while (*str && bidx < sizeof(buffer) - 1) {
               buffer[bidx++] = *str++;
            }// while
         } else if (fmt[i] == 'c') {
            char c = (char)va_arg(args, int);
            buffer[bidx++] = c;
         } else if (fmt[i] == 'd' || fmt[i] == 'i') {
            int val = va_arg(args, int);
            bool neg = (val < 0);
            unsigned int uval = neg ? (unsigned int)(-(int64_t)val) : (unsigned int)val;
            char num_buf[16];
            int nidx = 0;
            if (uval == 0) {
               num_buf[nidx++] = '0';
            } else {
               while (uval > 0 && nidx < 15) {
                  num_buf[nidx++] = '0' + (uval % 10);
                  uval /= 10;
               }// while
            }// if
            if (neg && bidx < sizeof(buffer) - 1) {
               buffer[bidx++] = '-';
            }// if
            while (nidx > 0 && bidx < sizeof(buffer) - 1) {
               buffer[bidx++] = num_buf[--nidx];
            }// while
         } else if (fmt[i] == 'u') {
            unsigned int uval = va_arg(args, unsigned int);
            char num_buf[16];
            int nidx = 0;
            if (uval == 0) {
               num_buf[nidx++] = '0';
            } else {
               while (uval > 0 && nidx < 15) {
                  num_buf[nidx++] = '0' + (uval % 10);
                  uval /= 10;
               }// while
            }// if
            while (nidx > 0 && bidx < sizeof(buffer) - 1) {
               buffer[bidx++] = num_buf[--nidx];
            }// while
         } else if (fmt[i] == 'x' || fmt[i] == 'X') {
            unsigned int hval = va_arg(args, unsigned int);
            char hex_buf[16];
            int hidx = 0;
            char base_char = (fmt[i] == 'X') ? 'A' : 'a';
            if (hval == 0) {
               hex_buf[hidx++] = '0';
            } else {
               while (hval > 0 && hidx < 16) {
                  uint8_t nibble = hval & 0xFU;
                  hex_buf[hidx++] = (nibble < 10) ? ('0' + nibble) : (base_char + (nibble - 10));
                  hval >>= 4;
               }// while
            }// if
            while (hidx > 0 && bidx < sizeof(buffer) - 1) {
               buffer[bidx++] = hex_buf[--hidx];
            }// while
         } else if (fmt[i] == 'p') {
            uintptr_t pval = (uintptr_t)va_arg(args, void *);
            if (bidx < sizeof(buffer) - 2) {
               buffer[bidx++] = '0';
               buffer[bidx++] = 'x';
            }// if
            char hex_buf[16];
            int hidx = 0;
            if (pval == 0) {
               hex_buf[hidx++] = '0';
            } else {
               while (pval > 0 && hidx < 16) {
                  uint8_t nibble = pval & 0xFU;
                  hex_buf[hidx++] = (nibble < 10) ? ('0' + nibble) : ('a' + (nibble - 10));
                  pval >>= 4;
               }// while
            }// if
            while (hidx > 0 && bidx < sizeof(buffer) - 1) {
               buffer[bidx++] = hex_buf[--hidx];
            }// while
         } else if (fmt[i] == '%') {
            buffer[bidx++] = '%';
         } else {
            /* Unsupported specifier - emit raw */
            buffer[bidx++] = '%';
            if (bidx < sizeof(buffer) - 1) {
               buffer[bidx++] = fmt[i];
            }// if
         }// if
      } else {
         buffer[bidx++] = fmt[i];
      }// if
   }// for

   va_end(args);
   buffer[bidx] = '\0';
   print(buffer);
   return (int)bidx;
}// linux_printk

static void *linux_ioremap(uintptr_t phys_addr, size_t size) {
   if (phys_addr == 0 || size == 0 || phys_addr + size < phys_addr) {
      errorn("kmi: invalid or overflowing MMIO physical address range");
      return NULL;
   }// if

   uintptr_t page_start = phys_addr & ~0xFFFU;
   uintptr_t page_end = (phys_addr + size + 4095U) & ~0xFFFU;
   for (uintptr_t p = page_start; p < page_end; p += 4096) {
      pkcall_map_device(p, p);
   }// for

   return (void *)phys_addr;
}// linux_ioremap

static void linux_iounmap(volatile void *addr) {
   (void)addr;
}// linux_iounmap

struct kmi_irq_slot {
   int (*handler)(int irq, void *dev);
   void *dev;
   bool active;
};

static struct kmi_irq_slot s_kmi_irqs[16];

static void kmi_irq_dispatch(uint32_t irq) {
   if (irq < 16 && s_kmi_irqs[irq].active && s_kmi_irqs[irq].handler) {
      s_kmi_irqs[irq].handler((int)irq, s_kmi_irqs[irq].dev);
   }// if
}// kmi_irq_dispatch

#define DECLARE_TRAMPOLINE(n) \
static void kmi_irq_trampoline_##n(void) { kmi_irq_dispatch(n); }

DECLARE_TRAMPOLINE(0)
DECLARE_TRAMPOLINE(1)
DECLARE_TRAMPOLINE(2)
DECLARE_TRAMPOLINE(3)
DECLARE_TRAMPOLINE(4)
DECLARE_TRAMPOLINE(5)
DECLARE_TRAMPOLINE(6)
DECLARE_TRAMPOLINE(7)
DECLARE_TRAMPOLINE(8)
DECLARE_TRAMPOLINE(9)
DECLARE_TRAMPOLINE(10)
DECLARE_TRAMPOLINE(11)
DECLARE_TRAMPOLINE(12)
DECLARE_TRAMPOLINE(13)
DECLARE_TRAMPOLINE(14)
DECLARE_TRAMPOLINE(15)

static handler_t s_trampolines[16] = {
   kmi_irq_trampoline_0,
   kmi_irq_trampoline_1,
   kmi_irq_trampoline_2,
   kmi_irq_trampoline_3,
   kmi_irq_trampoline_4,
   kmi_irq_trampoline_5,
   kmi_irq_trampoline_6,
   kmi_irq_trampoline_7,
   kmi_irq_trampoline_8,
   kmi_irq_trampoline_9,
   kmi_irq_trampoline_10,
   kmi_irq_trampoline_11,
   kmi_irq_trampoline_12,
   kmi_irq_trampoline_13,
   kmi_irq_trampoline_14,
   kmi_irq_trampoline_15
};

static int linux_request_irq(uint32_t irq, void *handler, uint32_t flags, const char *name, void *dev) {
   (void)flags;
   (void)name;
   if (irq == 7 || irq >= 16 || !handler) {
      errorn("kmi: invalid IRQ, timer protected, or NULL handler");
      return -1;
   }// if

   if (s_kmi_irqs[irq].active) {
      errorn("kmi: IRQ already assigned to active driver");
      return -1;
   }// if

   s_kmi_irqs[irq].handler = (int (*)(int, void *))handler;
   s_kmi_irqs[irq].dev = dev;
   s_kmi_irqs[irq].active = true;

   interrupt_handlers[irq] = s_trampolines[irq];
   successn("kmi: registered argument-preserving handler for IRQ");
   return 0;
}// linux_request_irq

static void linux_free_irq(uint32_t irq, void *dev) {
   if (irq < 16 && irq != 7 && s_kmi_irqs[irq].active) {
      if (s_kmi_irqs[irq].dev != dev) {
         errorn("kmi: free_irq dev pointer mismatch");
         return;
      }// if
      s_kmi_irqs[irq].active = false;
      s_kmi_irqs[irq].handler = NULL;
      s_kmi_irqs[irq].dev = NULL;
      interrupt_handlers[irq] = unknown_error;
   }// if
}// linux_free_irq

static int linux_pci_enable_device(void *dev) {
   (void)dev;
   return 0;
}// linux_pci_enable_device

static int linux_pci_register_driver(void *drv) {
   (void)drv;
   infon("kmi: PCI driver registered from external Linux module");
   return 0;
}// linux_pci_register_driver

static const struct kmi_symbol kmi_symtab[] = {
   { "printk", (void *)linux_printk },
   { "kmalloc", (void *)linux_kmalloc },
   { "kzalloc", (void *)linux_kzalloc },
   { "kfree", (void *)linux_kfree },
   { "ioremap", (void *)linux_ioremap },
   { "iounmap", (void *)linux_iounmap },
   { "spin_lock_init", (void *)linux_spin_lock_init },
   { "spin_lock", (void *)linux_spin_lock },
   { "spin_unlock", (void *)linux_spin_unlock },
   { "request_irq", (void *)linux_request_irq },
   { "free_irq", (void *)linux_free_irq },
   { "pci_enable_device", (void *)linux_pci_enable_device },
   { "pci_register_driver", (void *)linux_pci_register_driver },
   { "memcpy", (void *)memcpy },
   { "memset", (void *)memset },
   { "strcmp", (void *)strcmp },
   { NULL, NULL }
};

void *linux_kmi_lookup_symbol(const char *name) {
   if (!name) {
      return NULL;
   }// if
   for (size_t i = 0; kmi_symtab[i].name != NULL; i++) {
      if (strcmp(kmi_symtab[i].name, name) == 0) {
         return kmi_symtab[i].addr;
      }// if
   }// for
   return NULL;
}// linux_kmi_lookup_symbol

#endif
