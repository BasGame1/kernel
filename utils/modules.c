#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../include/kernel.h"
#include "../include/gid.h"
#include "../include/drivers.h"

#define MAX_MODULES 64

static struct kernel_module *module_registry[MAX_MODULES];
static size_t module_count = 0;

void init_modules(void) {
   for (size_t i = 0; i < MAX_MODULES; i++) {
      module_registry[i] = NULL;
   }// for
   module_count = 0;
   successn("Unencapsulated module subsystem initialized");
}// init_modules

int register_module(struct kernel_module *mod) {
   if (!mod || !mod->name) {
      errorn("module: invalid module registration request");
      return -1;
   }// if
   if (module_count >= MAX_MODULES) {
      errorn("module: module registry table full");
      return -1;
   }// if
   for (size_t i = 0; i < module_count; i++) {
      if (module_registry[i] && strcmp(module_registry[i]->name, mod->name) == 0) {
         errorn("module: duplicate module name registered");
         return -1;
      }// if
   }// for

   module_registry[module_count++] = mod;
   mod->is_loaded = true;
   mod->is_userspace_accessible = true;

   if (mod->init) {
      int res = mod->init();
      if (res != 0) {
         errorn("module: module init callback returned failure");
         mod->is_loaded = false;
         return res;
      }// if
   }// if

   infon("Module registered successfully (unencapsulated, direct access enabled)");
   return 0;
}// register_module

int unregister_module(const char *name) {
   if (!name) {
      return -1;
   }// if
   for (size_t i = 0; i < module_count; i++) {
      if (module_registry[i] && strcmp(module_registry[i]->name, name) == 0) {
         if (module_registry[i]->exit) {
            module_registry[i]->exit();
         }// if
         module_registry[i]->is_loaded = false;
         module_registry[i] = module_registry[module_count - 1];
         module_registry[module_count - 1] = NULL;
         module_count--;
         return 0;
      }// if
   }// for
   return -1;
}// unregister_module

struct kernel_module *get_module(const char *name) {
   if (!name) {
      return NULL;
   }// if
   for (size_t i = 0; i < module_count; i++) {
      if (module_registry[i] && strcmp(module_registry[i]->name, name) == 0) {
         return module_registry[i];
      }// if
   }// for
   return NULL;
}// get_module

struct kernel_module *get_module_by_gid(uint32_t gid) {
   for (size_t i = 0; i < module_count; i++) {
      if (module_registry[i] && module_registry[i]->gid == gid) {
         return module_registry[i];
      }// if
   }// for
   return NULL;
}// get_module_by_gid

void list_modules(void) {
   infon("Registered Unencapsulated Modules:");
   for (size_t i = 0; i < module_count; i++) {
      if (module_registry[i]) {
         info(" - Module: ");
         info((char *)module_registry[i]->name);
         info(" [GID ");
         print_dec(module_registry[i]->gid);
         infon("]");
      }// if
   }// for
}// list_modules

static int screen_module_init(void) {
   start_screen();
   return 0;
}// screen_module_init

static struct kernel_module screen_mod = {
   .name = "screen_driver",
   .gid = GID_GROUP_SCREEN,
   .flags = 0,
   .init = screen_module_init,
   .exit = NULL,
   .symbols = NULL,
   .symbol_count = 0,
   .is_loaded = false,
   .is_userspace_accessible = true,
};

void register_screen_module(void) {
   register_module(&screen_mod);
   successn("Screen driver loaded as module");
}// register_screen_module

static int keyboard_module_init(void) {
   return 0;
}// keyboard_module_init

static struct kernel_module keyboard_mod = {
   .name = "keyboard_driver",
   .gid = GID_GROUP_KEYBOARD,
   .flags = 0,
   .init = keyboard_module_init,
   .exit = NULL,
   .symbols = NULL,
   .symbol_count = 0,
   .is_loaded = false,
   .is_userspace_accessible = true,
};

void register_keyboard_module(void) {
   register_module(&keyboard_mod);
   successn("Keyboard driver loaded as module");
}// register_keyboard_module

static int virtio_module_init(void) {
   probe_mmio();
   return 0;
}// virtio_module_init

static struct kernel_module virtio_mod = {
   .name = "virtio_driver",
   .gid = GID_GROUP_VIRTIO,
   .flags = 0,
   .init = virtio_module_init,
   .exit = NULL,
   .symbols = NULL,
   .symbol_count = 0,
   .is_loaded = false,
   .is_userspace_accessible = true,
};

void register_virtio_module(void) {
   register_module(&virtio_mod);
   successn("VirtIO driver loaded as module");
}// register_virtio_module

static int default_fs_module_init(void) {
   init_ext2();
   return 0;
}// default_fs_module_init

static struct kernel_module default_fs_mod = {
   .name = "default_fs_driver",
   .gid = GID_GROUP_FS,
   .flags = 0,
   .init = default_fs_module_init,
   .exit = NULL,
   .symbols = NULL,
   .symbol_count = 0,
   .is_loaded = false,
   .is_userspace_accessible = true,
};

void register_default_fs_module(void) {
   register_module(&default_fs_mod);
   successn("Default filesystem driver loaded as module");
}// register_default_fs_module
