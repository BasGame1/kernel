#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../include/kernel.h"
#include "../include/userspace.h"
#include "../include/gid.h"
#include "../include/memory.h"
#include "../include/hardware.h"

#if defined(__USE_LINUX_MODULES__)

#if __has_include("../include/elf_loader.h")
#include "../include/elf_loader.h"
#else
#include "elf_loader.h"
#endif

#if __has_include("../include/linux_service.h")
#include "../include/linux_service.h"
#else
#include "linux_service.h"
#endif

#define MAX_USER_LINUX_MODULES 16

static struct linux_ko_module s_loaded_modules[MAX_USER_LINUX_MODULES];
static size_t s_loaded_count = 0;

static volatile uint8_t s_svc_lock = 0;

static void svc_lock_acquire(void) {
   while (__atomic_test_and_set(&s_svc_lock, __ATOMIC_ACQUIRE)) {
     // wait
   }// while
}// svc_lock_acquire

static void svc_lock_release(void) {
   __atomic_clear(&s_svc_lock, __ATOMIC_RELEASE);
}// svc_lock_release

struct linux_fast_dispatch g_linux_fast_dispatch;

static void init_fast_dispatch(void) {
   g_linux_fast_dispatch.kmalloc = (void *(*)(size_t, int))linux_kmi_lookup_symbol("kmalloc");
   g_linux_fast_dispatch.kzalloc = (void *(*)(size_t, int))linux_kmi_lookup_symbol("kzalloc");
   g_linux_fast_dispatch.kfree = (void (*)(const void *))linux_kmi_lookup_symbol("kfree");
   g_linux_fast_dispatch.printk = (int (*)(const char *, ...))linux_kmi_lookup_symbol("printk");
   g_linux_fast_dispatch.ioremap = (void *(*)(uintptr_t, size_t))linux_kmi_lookup_symbol("ioremap");
   g_linux_fast_dispatch.iounmap = (void (*)(volatile void *))linux_kmi_lookup_symbol("iounmap");
   g_linux_fast_dispatch.spin_lock = (void (*)(void *))linux_kmi_lookup_symbol("spin_lock");
   g_linux_fast_dispatch.spin_unlock = (void (*)(void *))linux_kmi_lookup_symbol("spin_unlock");
   g_linux_fast_dispatch.request_irq = (int (*)(uint32_t, void *, uint32_t, const char *, void *))linux_kmi_lookup_symbol("request_irq");
   g_linux_fast_dispatch.free_irq = (void (*)(uint32_t, void *))linux_kmi_lookup_symbol("free_irq");

   if (!g_linux_fast_dispatch.kmalloc || !g_linux_fast_dispatch.kzalloc ||
       !g_linux_fast_dispatch.kfree || !g_linux_fast_dispatch.printk ||
       !g_linux_fast_dispatch.ioremap || !g_linux_fast_dispatch.iounmap ||
       !g_linux_fast_dispatch.spin_lock || !g_linux_fast_dispatch.spin_unlock ||
       !g_linux_fast_dispatch.request_irq || !g_linux_fast_dispatch.free_irq) {
      errorn("linux_svc: one or more KMI fast dispatch symbols could not be resolved");
   }// if
}// init_fast_dispatch

int user_load_linux_module(const void *ko_data, size_t size) {
   if (!ko_data || size == 0 || size > 16 * 1024 * 1024) {
      errorn("linux_svc: invalid module payload or out-of-bounds size");
      return -1;
   }// if

   svc_lock_acquire();

   if (s_loaded_count >= MAX_USER_LINUX_MODULES) {
      errorn("linux_svc: maximum user Linux module capacity reached");
      svc_lock_release();
      return -1;
   }// if

   struct linux_ko_module *target_slot = &s_loaded_modules[s_loaded_count];
   memset(target_slot, 0, sizeof(*target_slot));

   int ret = load_linux_ko(ko_data, size, target_slot);
   if (ret != 0) {
      errorn("linux_svc: ELF loader returned failure");
      memset(target_slot, 0, sizeof(*target_slot));
      svc_lock_release();
      return ret;
   }// if

   for (size_t i = 0; i < s_loaded_count; i++) {
      if (s_loaded_modules[i].is_loaded && strcmp(s_loaded_modules[i].name, target_slot->name) == 0) {
         errorn("linux_svc: module with duplicate name already loaded");
         unload_linux_ko(target_slot);
         memset(target_slot, 0, sizeof(*target_slot));
         svc_lock_release();
         return -1;
      }// if
   }// for

   s_loaded_count++;
   svc_lock_release();

   successn("linux_svc: user Linux module loaded and verified successfully");
   return 0;
}// user_load_linux_module

int user_unload_linux_module(const char *name) {
   if (!name || name[0] == '\0') {
      errorn("linux_svc: NULL or empty module name specified for unload");
      return -1;
   }// if

   svc_lock_acquire();

   int found_idx = -1;
   for (size_t i = 0; i < s_loaded_count; i++) {
      if (s_loaded_modules[i].is_loaded && strcmp(s_loaded_modules[i].name, name) == 0) {
         found_idx = (int)i;
         break;
      }// if
   }// for

   if (found_idx < 0) {
      errorn("linux_svc: module not found in loaded module table");
      svc_lock_release();
      return -1;
   }// if

   int ret = unload_linux_ko(&s_loaded_modules[found_idx]);
   if (ret != 0) {
      errorn("linux_svc: unload_linux_ko returned error");
      svc_lock_release();
      return ret;
   }// if

   for (size_t j = (size_t)found_idx; j + 1 < s_loaded_count; j++) {
      s_loaded_modules[j] = s_loaded_modules[j + 1];
   }// for
   s_loaded_count--;
   memset(&s_loaded_modules[s_loaded_count], 0, sizeof(struct linux_ko_module));

   svc_lock_release();
   successn("linux_svc: module successfully unloaded from service");
   return 0;
}// user_unload_linux_module

void linux_module_service_main(void) {
   infon("linux_svc: starting userspace module manager event loop");
   init_fast_dispatch();

   struct linux_mod_ipc_req req;
   struct linux_mod_ipc_resp resp;

   while (1) {
      memset(&req, 0, sizeof(req));
      memset(&resp, 0, sizeof(resp));

      uint32_t ret = sys_ipc_recv(0, &req);
      if (ret == (uint32_t)-1) {
         test_call();
         continue;
      }// if

      req.name[sizeof(req.name) - 1] = '\0';

      switch (req.cmd) {
         case LINUX_MOD_CMD_LOAD:
            if (!req.data_ptr || req.data_len == 0 || req.data_len > 16 * 1024 * 1024 ||
                (uintptr_t)req.data_ptr + req.data_len < (uintptr_t)req.data_ptr) {
               resp.status = -1;
               errorn("linux_svc: invalid payload parameters for IPC LOAD");
            } else {
               resp.status = user_load_linux_module(req.data_ptr, req.data_len);
            }// if
            break;

         case LINUX_MOD_CMD_UNLOAD:
            resp.status = user_unload_linux_module(req.name);
            break;

         case LINUX_MOD_CMD_LOOKUP:
            resp.symbol_addr = linux_kmi_lookup_symbol(req.name);
            resp.status = (resp.symbol_addr != NULL) ? 0 : -1;
            break;

         case LINUX_MOD_CMD_LIST:
            svc_lock_acquire();
            resp.module_count = (uint32_t)s_loaded_count;
            svc_lock_release();
            resp.status = 0;
            break;

         default:
            resp.status = -1;
            errorn("linux_svc: unknown IPC command code");
            break;
      }// switch

      sys_ipc_send(ret, &resp);
   }// while
}// linux_module_service_main

void create_linux_module_process(void) {
   infon("linux_svc: launching userspace Linux module microkernel service");
   extern uint32_t create_process(void *entry, uint32_t gid);
   extern struct process *get_process_by_pid(uint32_t pid);
   extern uint32_t generate_sid(uint32_t gid, uintptr_t entry);

   uint32_t pid = create_process((void *)linux_module_service_main, LINUX_MODULE_GID);
   if (pid == (uint32_t)-1) {
      errorn("linux_svc: failed to create service process");
      return;
   }// if

   struct process *proc = get_process_by_pid(pid);
   if (!proc) {
      errorn("linux_svc: failed to locate created process descriptor");
      return;
   }// if

   proc->sid = generate_sid(LINUX_MODULE_GID, (uintptr_t)linux_module_service_main);
   successn("linux_svc: userspace Linux module service process running");
}// create_linux_module_process

#endif
