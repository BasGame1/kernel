/*
 * Ported from the Linux kernel (drivers/tty/serial/amba-pl011.c)
 * Copyright (C) Linus Torvalds and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include "../include/drivers.h"

#define UART011_DR 0x00
#define UART011_RSR 0x04
#define UART011_FR 0x18
#define UART011_ILPR 0x20
#define UART011_IBRD 0x24
#define UART011_FBRD 0x28
#define UART011_LCRH 0x2C
#define UART011_CR 0x30
#define UART011_IFLS 0x34
#define UART011_IMSC 0x38
#define UART011_MIS 0x40
#define UART011_ICR 0x44
#define UART011_FR_TXFF (1U << 5)
#define UART011_FR_RXFE (1U << 4)
#define UART011_FR_BUSY (1U << 3)
#define UART011_CR_UARTEN (1U << 0)
#define UART011_CR_TXE (1U << 8)
#define UART011_CR_RXE (1U << 9)
#define UART011_LCRH_WLEN_8 (3U << 5)
#define UART011_LCRH_FEN (1U << 4)

static inline uint32_t pl011_read(uintptr_t base, uint32_t offset) {
   return *(volatile uint32_t *)(base + offset);
}// pl011_read

static inline void pl011_write(uintptr_t base, uint32_t offset, uint32_t val) {
   *(volatile uint32_t *)(base + offset) = val;
}// pl011_write

void pl011_init(uintptr_t base_addr) {
   pl011_write(base_addr, UART011_CR, 0);
   pl011_write(base_addr, UART011_ICR, 0x7FF);
   pl011_write(base_addr, UART011_LCRH, UART011_LCRH_WLEN_8 | UART011_LCRH_FEN);
   pl011_write(base_addr, UART011_CR, UART011_CR_UARTEN | UART011_CR_TXE | UART011_CR_RXE);
}// pl011_init

void pl011_putc(uintptr_t base_addr, char c) {
   while (pl011_read(base_addr, UART011_FR) & UART011_FR_TXFF) {
      // wait
   }// while
   pl011_write(base_addr, UART011_DR, (uint32_t)c);
}// pl011_putc

char pl011_getc(uintptr_t base_addr) {
   while (pl011_read(base_addr, UART011_FR) & UART011_FR_RXFE) {
      // wait
   }// while
   return (char)(pl011_read(base_addr, UART011_DR) & 0xFF);
}// pl011_getc

bool pl011_has_data(uintptr_t base_addr) {
   return (pl011_read(base_addr, UART011_FR) & UART011_FR_RXFE) == 0;
}// pl011_has_data
