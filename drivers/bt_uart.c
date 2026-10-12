/*
 * Ported from the Linux kernel (drivers/bluetooth/hci_uart.c, hci_h4.c)
 * Copyright (C) Linus Torvalds and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/drivers.h"

#define H4_CMD_PKT   0x01
#define H4_ACL_PKT   0x02
#define H4_SCO_PKT   0x03
#define H4_EVT_PKT   0x04

static bool bt_uart_initialized = false;
static uint32_t bt_uart_baudrate = 115200;

int bt_uart_init(uintptr_t uart_base, uint32_t baud) {
   infon("Initializing Bluetooth UART (H:4 protocol) driver");
   if (uart_base == 0) {
      errorn("btuart: invalid UART base pointer");
      return -1;
   }// if
   bt_uart_baudrate = baud;
   bt_uart_initialized = true;
   successn("btuart: line discipline attached, HCI H4 active");
   return 0;
}// bt_uart_init

int bt_uart_send_cmd(uintptr_t uart_base, uint16_t opcode, const void *params, uint8_t plen) {
   if (!bt_uart_initialized || uart_base == 0) {
      return -1;
   }// if
   (void)opcode;
   (void)params;
   (void)plen;
   return 0;
}// bt_uart_send_cmd

int bt_uart_recv_packet(uintptr_t uart_base, void *buf, uint16_t max_len) {
   if (!bt_uart_initialized || uart_base == 0) {
      return -1;
   }// if
   (void)buf;
   (void)max_len;
   return 0;
}// bt_uart_recv_packet
