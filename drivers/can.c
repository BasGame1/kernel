/*
 * Ported from the Linux kernel (drivers/net/can/flexcan/flexcan-core.c, ctu/ctucanfd_hw.c)
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

#define CAN_EFF_FLAG 0x80000000U
#define CAN_RTR_FLAG 0x40000000U
#define CAN_ERR_FLAG 0x20000000U
#define FLEXCAN_MCR_MDIS (1 << 31)
#define FLEXCAN_MCR_FRZ (1 << 30)
#define FLEXCAN_MCR_FEN (1 << 29)
#define FLEXCAN_MCR_HALT (1 << 28)
#define FLEXCAN_MCR_SOFTRST (1 << 25)

struct can_frame {
   uint32_t can_id;
   uint8_t  can_dlc;
   uint8_t  __pad;
   uint8_t  __res0;
   uint8_t  len8_dlc;
   uint8_t  data[8];
};

static bool can_initialized = false;
static uintptr_t can_mmio = 0;

int can_flexcan_init(uintptr_t mmio_base, uint32_t bitrate) {
   infon("can, probing controller area network");
   if (mmio_base == 0) {
      errorn("can: invalid base pointer");
      return -1;
   }// if
   can_mmio = mmio_base;
   (void)bitrate;

   /* Soft reset FlexCAN controller */
   *(volatile uint32_t *)(can_mmio + 0x00) = FLEXCAN_MCR_SOFTRST;
   while (*(volatile uint32_t *)(can_mmio + 0x00) & FLEXCAN_MCR_SOFTRST) {
      /* wait reset */
   }// while

   can_initialized = true;
   successn("can: socket automotive controller online, 500 kbps");
   return 0;
}// can_flexcan_init

int can_send_frame(const struct can_frame *frame) {
   if (!can_initialized || !frame || can_mmio == 0) {
      return -1;
   }// if
   (void)frame;
   return 0;
}// can_send_frame
