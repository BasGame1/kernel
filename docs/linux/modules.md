# Linux Kernel Module

## Overview
Includes compatibility to linux kernel modules, to active use the config `USE_LINUX_MODULES`

Running as a userspace with GID 16, exposes directly the pointers for speed while not being much less secure for this

The system counts on two parts, the first one loads the elf and anaylizes, the second one gets its symbols and applys the correct calls

---

## Architectural Design

### Fast Dispatch Table (`struct linux_fast_dispatch`)

```c
struct linux_fast_dispatch {
   void *(*kmalloc)(size_t size, int flags);
   void *(*kzalloc)(size_t size, int flags);
   void (*kfree)(const void *ptr);
   int (*printk)(const char *fmt, ...);
   void *(*ioremap)(uintptr_t phys_addr, size_t size);
   void (*iounmap)(volatile void *addr);
   void (*spin_lock)(void *sl);
   void (*spin_unlock)(void *sl);
   int (*request_irq)(uint32_t irq, void *handler, uint32_t flags, const char *name, void *dev);
   void (*free_irq)(uint32_t irq, void *dev);
};
```

## Kernel Module Interface

### Exported Symbols
The KMI symbol dictionary exports core Linux driver APIs:
- Memory: `kmalloc`, `kzalloc`, `kfree`
- Logging: `printk`
- Hardware: `ioremap`, `iounmap`, `request_irq`, `free_irq`
- Synchronization: `spin_lock`, `spin_unlock`

### Fast Hash Lookups
Symbol resolution utilizes an FNV1a hash lookup table rather than linear string scanning, providing near O(1) resolution speed during module load.

---

## Module Lifecycle Management

### Data Structure (`struct linux_ko_module`)
```c
struct linux_ko_module {
   char name[MODULE_NAME_LEN];
   void *base_address;
   size_t total_size;
   struct module_allocation allocs[MAX_MODULE_ALLOCATIONS];
   size_t alloc_count;
   int (*init_module)(void);
   void (*cleanup_module)(void);
   uint32_t refcnt;
   uint32_t state;
   bool is_loaded;
};
```

### Loading Module
```c
int load_linux_ko(const void *elf_data, size_t data_len, struct linux_ko_module *out_mod);
```
- Maps sections, applies relocations, binds `init_module` and `cleanup_module` pointers.
- Invokes `init_module()`. If initialization is non-zero, automatically cleans up and frees allocations.

### unloading Module
```c
int unload_linux_ko(struct linux_ko_module *mod);
```
- Asserts module reference count `refcnt == 0`.
- Executes `cleanup_module()` to unregister device drivers and free interrupt lines.
- Unmaps all loaded sections and zeros allocation tables.
