/*
 * Ported from the Linux kernel (drivers/tty/serial/8250/)
 * Copyright (C) Linus Torvalds and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include "../include/drivers.h"

#define UART_RBR_OFFSET 0
#define UART_THR_OFFSET 0
#define UART_IER_OFFSET 1
#define UART_IIR_OFFSET 2
#define UART_FCR_OFFSET 2
#define UART_LCR_OFFSET 3
#define UART_MCR_OFFSET 4
#define UART_LSR_OFFSET 5
#define UART_MSR_OFFSET 6
#define UART_SCR_OFFSET 7
#define UART_DLL_OFFSET 0
#define UART_DLM_OFFSET 1
#define UART_LSR_DR 0x01
#define UART_LSR_THRE 0x20
#define UART_LCR_DLAB 0x80
#define UART_LCR_WLEN8 0x03
#define UART_FCR_ENABLE 0x01
#define UART_FCR_CLEAR 0x06

static inline uint8_t serial8250_in(uintptr_t base, uint32_t offset) {
   return *(volatile uint8_t *)(base + offset);
}// serial8250_in

static inline void serial8250_out(uintptr_t base, uint32_t offset, uint8_t val) {
   *(volatile uint8_t *)(base + offset) = val;
}// serial8250_out

void uart_8250_init(uintptr_t base_addr, uint32_t clock_hz, uint32_t baud) {
   serial8250_out(base_addr, UART_IER_OFFSET, 0x00);
   uint32_t divisor = (clock_hz + (baud * 8)) / (baud * 16);
   if (divisor == 0) {
      divisor = 1;
   }// if
   serial8250_out(base_addr, UART_LCR_OFFSET, UART_LCR_DLAB);
   serial8250_out(base_addr, UART_DLL_OFFSET, (uint8_t)(divisor & 0xFF));
   serial8250_out(base_addr, UART_DLM_OFFSET, (uint8_t)((divisor >> 8) & 0xFF));
   serial8250_out(base_addr, UART_LCR_OFFSET, UART_LCR_WLEN8);
   serial8250_out(base_addr, UART_FCR_OFFSET, UART_FCR_ENABLE | UART_FCR_CLEAR);
   serial8250_out(base_addr, UART_MCR_OFFSET, 0x03);
}// uart_8250_init

void uart_8250_putc(uintptr_t base_addr, char c) {
   while ((serial8250_in(base_addr, UART_LSR_OFFSET) & UART_LSR_THRE) == 0) {
      // wait
   }// while
   serial8250_out(base_addr, UART_THR_OFFSET, (uint8_t)c);
}// uart_8250_putc

char uart_8250_getc(uintptr_t base_addr) {
   while ((serial8250_in(base_addr, UART_LSR_OFFSET) & UART_LSR_DR) == 0) {
      // wait
   }// while
   return (char)serial8250_in(base_addr, UART_RBR_OFFSET);
}// uart_8250_getc

bool uart_8250_has_data(uintptr_t base_addr) {
   return (serial8250_in(base_addr, UART_LSR_OFFSET) & UART_LSR_DR) != 0;
}// uart_8250_has_data
