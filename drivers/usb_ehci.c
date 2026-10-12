/*
 * Ported from the Linux kernel (drivers/usb/host/ehci-hcd.c, ehci-q.c)
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

#define EHCI_CAPLENGTH 0x00
#define EHCI_HCIVERSION 0x02
#define EHCI_HCSPARAMS 0x04
#define EHCI_HCCPARAMS 0x08
#define EHCI_USBCMD 0x00
#define EHCI_USBSTS 0x04
#define EHCI_USBINTR 0x08
#define EHCI_FRINDEX 0x0C
#define EHCI_CTRLDSSEGMENT 0x10
#define EHCI_PERIODICLISTBASE 0x14
#define EHCI_ASYNCLISTADDR 0x18
#define EHCI_CONFIGFLAG 0x40
#define EHCI_PORTSC 0x44
#define EHCI_CMD_RUN (1 << 0)
#define EHCI_CMD_RESET (1 << 1)
#define EHCI_STS_HALTED (1 << 12)

static bool ehci_initialized = false;

int usb_ehci_init(uintptr_t bar_base) {
   infon("Probing USB 2.0 EHCI host controller");
   if (bar_base == 0) {
      errorn("ehci: invalid BAR address");
      return -1;
   }// if

   uint8_t caplen = *(volatile uint8_t *)(bar_base + EHCI_CAPLENGTH);
   uintptr_t op_base = bar_base + caplen;

   volatile uint32_t *usbcmd = (volatile uint32_t *)(op_base + EHCI_USBCMD);
   volatile uint32_t *usbsts = (volatile uint32_t *)(op_base + EHCI_USBSTS);
   (void)usbsts;

   *usbcmd |= EHCI_CMD_RESET;
   while (*usbcmd & EHCI_CMD_RESET) {
      // wait
   }// while

   ehci_initialized = true;
   successn("ehci: USB 2.0 High-Speed controller initialized");
   return 0;
}// usb_ehci_init

bool usb_ehci_is_ready(void) {
   return ehci_initialized;
}// usb_ehci_is_ready
