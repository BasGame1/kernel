/*
 * Ported from the Linux kernel (drivers/gpu/drm/msm/adreno/adreno_gpu.c, a6xx_gpu.c)
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

#define REG_A6XX_RBBM_STATUS            0x0210
#define REG_A6XX_RBBM_STATUS3           0x0213
#define REG_A6XX_CP_RB_BASE             0x0800
#define REG_A6XX_CP_RB_CNTL             0x0804
#define REG_A6XX_CP_RB_RPTR             0x0806
#define REG_A6XX_CP_RB_WPTR             0x0807

static bool adreno_initialized = false;
static uintptr_t adreno_base = 0;
static uint32_t adreno_chip_id = 0;

static inline void qcom_wr(uintptr_t base, uint32_t reg, uint32_t val) {
   *(volatile uint32_t *)(base + (reg * 4)) = val;
}// qcom_wr

static inline uint32_t qcom_rd(uintptr_t base, uint32_t reg) {
   return *(volatile uint32_t *)(base + (reg * 4));
}// qcom_rd

int gpu_msm_adreno_init(uintptr_t mmio_base, uint32_t chip_id) {
   infon("Probing Qualcomm Snapdragon Adreno GPU");
   if (mmio_base == 0) {
      errorn("adreno: invalid MMIO base pointer");
      return -1;
   }// if
   adreno_base = mmio_base;
   adreno_chip_id = chip_id ? chip_id : 0x06030000;
   uint32_t rbbm = qcom_rd(adreno_base, REG_A6XX_RBBM_STATUS);
   if (rbbm == 0xFFFFFFFF) {
      errorn("adreno: GPU power domain clock disabled");
      return -1;
   }// if

   adreno_initialized = true;
   successn("adreno: Qualcomm Adreno graphics accelerator online");
   return 0;
}// gpu_msm_adreno_init

bool gpu_msm_adreno_is_active(void) {
   return adreno_initialized;
}// gpu_msm_adreno_is_active
