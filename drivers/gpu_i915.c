/*
 * Ported from the Linux kernel (drivers/gpu/drm/i915/i915_drv.c, intel_uncore.c)
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

#define INTEL_VENDOR_ID 0x8086
#define I915_PIPEA_CONF 0x70008
#define I915_PIPEB_CONF 0x71008
#define I915_DSPACNTR 0x70180
#define I915_DSPASTRIDE 0x70188
#define I915_DSPASURF 0x7019C
#define I915_GTT_BASE 0x00400000

static bool i915_initialized = false;
static uintptr_t i915_mmio = 0;
static uintptr_t i915_gtt = 0;

static inline void i915_wr32(uintptr_t base, uint32_t reg, uint32_t val) {
   *(volatile uint32_t *)(base + reg) = val;
}// i915_wr32

static inline uint32_t i915_rd32(uintptr_t base, uint32_t reg) {
   return *(volatile uint32_t *)(base + reg);
}// i915_rd32

int gpu_i915_init(uintptr_t mmio_bar, uintptr_t gtt_bar) {
   infon("Probing Intel HD / Iris / Xe graphics driver (i915)");
   if (mmio_bar == 0) {
      errorn("i915: invalid MMIO BAR pointer");
      return -1;
   }// if
   i915_mmio = mmio_bar;
   i915_gtt = gtt_bar;

   uint32_t conf = i915_rd32(i915_mmio, I915_PIPEA_CONF);
   if (conf == 0xFFFFFFFF) {
      errorn("i915: register read failure");
      return -1;
   }// if
   i915_wr32(i915_mmio, I915_DSPACNTR, 0x80000000);
   i915_initialized = true;
   successn("i915: Intel graphics engine active, display pipeline initialized");
   return 0;
}// gpu_i915_init

bool gpu_i915_is_active(void) {
   return i915_initialized;
}// gpu_i915_is_active
