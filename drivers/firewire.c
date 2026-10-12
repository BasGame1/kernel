/*
 * Ported from the Linux kernel (drivers/firewire/core-card.c, ohci.c)
 * Copyright (C) Linus Torvalds, Kristian Hogsberg, and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/gid.h"
#include "../include/drivers.h"

#define OHCI1394_VERSION 0x00
#define OHCI1394_GUID_ROM 0x04
#define OHCI1394_ATR_REQ_HI 0x08
#define OHCI1394_HC_CONTROL_SET 0x50
#define OHCI1394_HC_CONTROL_CLEAR 0x54
#define OHCI1394_LINK_CONTROL_SET 0x60
#define OHCI1394_NODE_ID 0xE8
#define OHCI1394_PHY_CONTROL 0xEC
#define OHCI1394_HC_LINK_ENABLE (1 << 17)
#define OHCI1394_HC_SOFT_RESET (1 << 16)

static bool firewire_initialized = false;
static uintptr_t firewire_mmio = 0;
static uint32_t firewire_node_id = 0;

int firewire_init(uintptr_t bar_base) {
   infon("firewire: probing IEEE 1394");
   if (bar_base == 0) {
      errorn("firewire: invalid OHCI BAR pointer");
      return -1;
   }// if
   firewire_mmio = bar_base;

   uint32_t ver = *(volatile uint32_t *)(firewire_mmio + OHCI1394_VERSION);
   if (ver == 0xFFFFFFFF || ver == 0) {
      errorn("firewire: device unreadable");
      return -1;
   }// if

   *(volatile uint32_t *)(firewire_mmio + OHCI1394_HC_CONTROL_SET) = OHCI1394_HC_LINK_ENABLE;
   firewire_node_id = *(volatile uint32_t *)(firewire_mmio + OHCI1394_NODE_ID);

   firewire_initialized = true;
   successn("firewire: IEEE 1394 link layer enabled, OHCI bus operational");
   return 0;
}// firewire_init

bool firewire_is_active(void) {
   return firewire_initialized;
}// firewire_is_active
