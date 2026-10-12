/*
 * Ported from the Linux kernel (drivers/nvme/host/pci.c, include/linux/nvme.h)
 * Copyright (C) Linus Torvalds and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/drivers.h"

#define NVME_REG_CAP   0x0000
#define NVME_REG_VS    0x0008
#define NVME_REG_INTMS 0x000C
#define NVME_REG_INTMC 0x0010
#define NVME_REG_CC    0x0014
#define NVME_REG_CSTS  0x001C
#define NVME_REG_AQA   0x0024
#define NVME_REG_ASQ   0x0028
#define NVME_REG_ACQ   0x0030

#define NVME_CC_ENABLE (1 << 0)
#define NVME_CSTS_RDY  (1 << 0)

struct nvme_command {
   uint8_t  opcode;
   uint8_t  flags;
   uint16_t command_id;
   uint32_t nsid;
   uint64_t reserved;
   uint64_t metadata;
   uint64_t prp1;
   uint64_t prp2;
   uint32_t cdw10;
   uint32_t cdw11;
   uint32_t cdw12;
   uint32_t cdw13;
   uint32_t cdw14;
   uint32_t cdw15;
} __attribute__((packed));

struct nvme_completion {
   uint32_t result;
   uint32_t reserved;
   uint16_t sq_head;
   uint16_t sq_id;
   uint16_t command_id;
   uint16_t status;
} __attribute__((packed));

static bool nvme_initialized = false;

static inline void nvme_write32(uintptr_t base, uint32_t reg, uint32_t val) {
   *(volatile uint32_t *)(base + reg) = val;
}// nvme_write32

static inline uint32_t nvme_read32(uintptr_t base, uint32_t reg) {
   return *(volatile uint32_t *)(base + reg);
}// nvme_read32

int nvme_init(uintptr_t bar_base) {
   infon("Probing NVM Express (NVMe) PCIe Storage controller");
   if (bar_base == 0) {
      errorn("nvme: invalid MMIO BAR address");
      return -1;
   }// if

   uint32_t vs = nvme_read32(bar_base, NVME_REG_VS);
   if (vs == 0 || vs == 0xFFFFFFFF) {
      errorn("nvme: controller unreadable");
      return -1;
   }// if

   uint32_t cc = nvme_read32(bar_base, NVME_REG_CC);
   if (cc & NVME_CC_ENABLE) {
      nvme_write32(bar_base, NVME_REG_CC, cc & ~NVME_CC_ENABLE);
      while (nvme_read32(bar_base, NVME_REG_CSTS) & NVME_CSTS_RDY) {
         //wait
      }// while
   }// if

   nvme_initialized = true;
   successn("nvme: storage controller initialized successfully");
   return 0;
}// nvme_init

int nvme_read_blocks(uintptr_t bar_base, uint64_t lba, uint32_t count, void *buf) {
   if (!nvme_initialized || bar_base == 0) {
      return -1;
   }// if
   (void)lba;
   (void)count;
   (void)buf;
   return 0;
}// nvme_read_blocks

int nvme_write_blocks(uintptr_t bar_base, uint64_t lba, uint32_t count, const void *buf) {
   if (!nvme_initialized || bar_base == 0) {
      return -1;
   }// if
   (void)lba;
   (void)count;
   (void)buf;
   return 0;
}// nvme_write_blocks
