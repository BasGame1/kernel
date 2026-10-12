/*
 * Ported from the Linux kernel (drivers/tty/serial/qcom_geni_serial.c)
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

#define SE_GENI_STATUS 0x0040
#define SE_GENI_TX_FIFOn 0x0700
#define SE_GENI_RX_FIFOn 0x0780
#define SE_GENI_TX_FIFO_STATUS 0x0800
#define SE_GENI_RX_FIFO_STATUS 0x0804
#define SE_UART_TX_TRANS_LEN 0x0270

static bool qcom_geni_initialized = false;
static uintptr_t qcom_geni_base = 0;

int serial_qcom_geni_init(uintptr_t mmio_base, uint32_t baud) {
   infon("Probing Qualcomm GENI Serial Engine (QCOM UART)");
   if (mmio_base == 0) {
      errorn("qcom-geni: invalid base address");
      return -1;
   }// if
   qcom_geni_base = mmio_base;
   (void)baud;

   qcom_geni_initialized = true;
   successn("qcom-geni: Qualcomm Snapdragon Serial Engine active");
   return 0;
}// serial_qcom_geni_init

void serial_qcom_geni_putc(char c) {
   if (!qcom_geni_initialized || qcom_geni_base == 0) {
      return;
   }// if
   *(volatile uint32_t *)(qcom_geni_base + SE_GENI_TX_FIFOn) = (uint8_t)c;
}// serial_qcom_geni_putc

char serial_qcom_geni_getc(void) {
   if (!qcom_geni_initialized || qcom_geni_base == 0) {
      return 0;
   }// if
   uint32_t rx_sts = *(volatile uint32_t *)(qcom_geni_base + SE_GENI_RX_FIFO_STATUS);
   if ((rx_sts & 0x7F) == 0) {
      return 0;
   }// if
   return (char)(*(volatile uint32_t *)(qcom_geni_base + SE_GENI_RX_FIFOn) & 0xFF);
}// serial_qcom_geni_getc
