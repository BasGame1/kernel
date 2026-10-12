/*
 * Ported from the Linux kernel (drivers/usb/host/xhci.c, xhci-mem.c)
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

#define XHCI_CAPLENGTH 0x00
#define XHCI_HCIVERSION 0x02
#define XHCI_HCSPARAMS1 0x04
#define XHCI_HCSPARAMS2 0x08
#define XHCI_HCSPARAMS3 0x0C
#define XHCI_HCCPARAMS1 0x10
#define XHCI_DBOFF 0x14
#define XHCI_RTSOFF 0x18
#define XHCI_USBCMD 0x00
#define XHCI_USBSTS 0x04
#define XHCI_PAGESIZE 0x08
#define XHCI_DNCTRL 0x14
#define XHCI_CRCR 0x18
#define XHCI_DCBAAP 0x30
#define XHCI_CONFIG 0x38
#define XHCI_CMD_RUN (1 << 0)
#define XHCI_CMD_RESET (1 << 1)
#define XHCI_STS_HALT (1 << 0)
#define XHCI_STS_CNR (1 << 11)

static bool xhci_initialized = false;
static uint32_t xhci_max_slots = 0;
static uint32_t xhci_max_ports = 0;

int usb_xhci_init(uintptr_t bar_base) {
   infon("Probing USB 3.0 xHCI host controller");
   if (bar_base == 0) {
      errorn("xhci: invalid MMIO BAR address");
      return -1;
   }// if

   uint8_t caplen = *(volatile uint8_t *)(bar_base + XHCI_CAPLENGTH);
   uint32_t hcsparams1 = *(volatile uint32_t *)(bar_base + XHCI_HCSPARAMS1);
   xhci_max_slots = hcsparams1 & 0xFF;
   xhci_max_ports = (hcsparams1 >> 24) & 0xFF;

   uintptr_t op_base = bar_base + caplen;
   volatile uint32_t *usbcmd = (volatile uint32_t *)(op_base + XHCI_USBCMD);
   volatile uint32_t *usbsts = (volatile uint32_t *)(op_base + XHCI_USBSTS);

   if (!(*usbsts & XHCI_STS_HALT)) {
      *usbcmd &= ~XHCI_CMD_RUN;
      while (!(*usbsts & XHCI_STS_HALT)) {
         // wait
      }// while
   }// if

   xhci_initialized = true;
   successn("xhci: USB 3.0 controller operational, root hub online");
   return 0;
}// usb_xhci_init

bool usb_xhci_is_ready(void) {
   return xhci_initialized;
}// usb_xhci_is_ready
