/*
 * Ported from the Linux kernel (drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c, amdgpu_device.c)
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

#define AMD_VENDOR_ID           0x1002
#define AMD_MMIO_CONFIG_REG     0x0000
#define AMD_GRBM_STATUS         0x8010
#define AMD_CRTC_CONTROL        0x6000
#define AMD_IH_CNTL             0x3E00

static bool amdgpu_initialized = false;
static uintptr_t amdgpu_mmio = 0;
static uintptr_t amdgpu_vram = 0;

static inline void amd_wr32(uintptr_t base, uint32_t reg, uint32_t val) {
   *(volatile uint32_t *)(base + reg) = val;
}// amd_wr32

static inline uint32_t amd_rd32(uintptr_t base, uint32_t reg) {
   return *(volatile uint32_t *)(base + reg);
}// amd_rd32

int gpu_amdgpu_init(uintptr_t mmio_bar, uintptr_t vram_bar) {
   infon("amdgpu: probbing");
   if (mmio_bar == 0) {
      errorn("amdgpu: invalid MMIO base pointer");
      return -1;
   }// if
   amdgpu_mmio = mmio_bar;
   amdgpu_vram = vram_bar;
   uint32_t grbm = amd_rd32(amdgpu_mmio, AMD_GRBM_STATUS);
   if (grbm == 0xFFFFFFFF) {
      errorn("amdgpu: device MMIO read timeout");
      return -1;
   }// if
   amd_wr32(amdgpu_mmio, AMD_IH_CNTL, 0x00000001);
   amdgpu_initialized = true;
   successn("amdgpu: Radeon GPU initialized, VRAM mapped");
   return 0;
}// gpu_amdgpu_init

bool gpu_amdgpu_is_active(void) {
   return amdgpu_initialized;
}// gpu_amdgpu_is_active
