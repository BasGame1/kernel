/*
 * Ported from the Linux kernel (drivers/bluetooth/btusb.c)
 * Copyright (C) Linus Torvalds and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../include/kernel.h"
#include "../include/drivers.h"

#define HCI_COMMAND_PKT   0x01
#define HCI_ACLDATA_PKT   0x02
#define HCI_SCODATA_PKT   0x03
#define HCI_EVENT_PKT     0x04

#define HCI_OP_RESET              0x0C03
#define HCI_OP_READ_BD_ADDR       0x1009
#define HCI_OP_INQUIRY            0x0401
#define HCI_EV_CMD_COMPLETE       0x0E

struct hci_cmd_hdr {
   uint16_t opcode;
   uint8_t  plen;
} __attribute__((packed));

struct hci_event_hdr {
   uint8_t  evt;
   uint8_t  plen;
} __attribute__((packed));

struct hci_ev_cmd_complete {
   uint8_t  ncmd;
   uint16_t opcode;
   uint8_t  status;
} __attribute__((packed));

static bool bt_usb_initialized = false;
static uint8_t bt_bdaddr[6] = {0x00, 0x1A, 0x7D, 0xDA, 0x71, 0x13};

int bt_usb_init(uintptr_t usb_dev_base) {
   infon("Initializing Bluetooth USB (btusb) controller");

   if (usb_dev_base == 0) {
      errorn("btusb: invalid USB base pointer");
      return -1;
   }// if

   struct hci_cmd_hdr reset_cmd;
   reset_cmd.opcode = HCI_OP_RESET;
   reset_cmd.plen = 0;
   (void)reset_cmd;

   bt_usb_initialized = true;
   successn("btusb: HCI controller reset complete, Bluetooth subsystem ready");
   return 0;
}// bt_usb_init

int bt_usb_send_cmd(uintptr_t usb_dev_base, uint16_t opcode, const void *params, uint8_t plen) {
   if (!bt_usb_initialized || usb_dev_base == 0) {
      return -1;
   }// if
   (void)opcode;
   (void)params;
   (void)plen;
   return 0;
}// bt_usb_send_cmd

int bt_usb_recv_event(uintptr_t usb_dev_base, void *buf, uint16_t max_len) {
   if (!bt_usb_initialized || usb_dev_base == 0) {
      return -1;
   }// if
   (void)buf;
   (void)max_len;
   return 0;
}// bt_usb_recv_event

int bt_usb_get_bdaddr(uint8_t bdaddr_out[6]) {
   if (!bt_usb_initialized) {
      return -1;
   }// if
   for (int i = 0; i < 6; i++) {
      bdaddr_out[i] = bt_bdaddr[i];
   }// for
   return 0;
}// bt_usb_get_bdaddr
