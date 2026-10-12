/*
 * Ported from the Linux kernel (drivers/gpu/drm/nouveau/nouveau_drm.c, nvkm/core/subdev.c)
 * Copyright (C) Linus Torvalds and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/gid.h"
#include "../include/drivers.h"

#define NV_PMC_BOOT_0 0x000000
#define NV_PMC_ENABLE 0x000200
#define NV_PBUS_PCI_NV_1 0x001804
#define NV_PRAMIN_OFFSET 0x700000
#define NV_PCRTC_BASE 0x600000
#define NV_VENDOR_ID 0x10DE

static bool nouveau_initialized = false;
static uintptr_t nouveau_bar0 = 0;
static uintptr_t nouveau_bar1 = 0;
static uint32_t nouveau_chipset = 0;

static inline void nv_wr32(uintptr_t base, uint32_t reg, uint32_t val) {
   *(volatile uint32_t *)(base + reg) = val;
}// nv_wr32

static inline uint32_t nv_rd32(uintptr_t base, uint32_t reg) {
   return *(volatile uint32_t *)(base + reg);
}// nv_rd32

int gpu_nouveau_init(uintptr_t mmio_bar0, uintptr_t fb_bar1) {
   infon("nouveau: probbing nvidia card");
   if (mmio_bar0 == 0) {
      errorn("nouveau: invalid BAR0 MMIO pointer");
      return -1;
   }// if
   nouveau_bar0 = mmio_bar0;
   nouveau_bar1 = fb_bar1;

   uint32_t boot0 = nv_rd32(nouveau_bar0, NV_PMC_BOOT_0);
   if (boot0 == 0 || boot0 == 0xFFFFFFFF) {
      errorn("nouveau: device unreadable or disconnected");
      return -1;
   }// if
   nouveau_chipset = (boot0 >> 20) & 0x1FF;
   nv_wr32(nouveau_bar0, NV_PMC_ENABLE, 0xFFFFFFFF);
   nouveau_initialized = true;
   successn("nouveau: activated");
   return 0;
}// gpu_nouveau_init

bool gpu_nouveau_is_active(void) {
   return nouveau_initialized;
}// gpu_nouveau_is_active

uint32_t gpu_nouveau_get_chipset(void) {
   return nouveau_chipset;
}// gpu_nouveau_get_chipset
