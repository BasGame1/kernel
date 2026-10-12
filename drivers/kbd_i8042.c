/*
 * Ported from the Linux kernel (drivers/input/serio/i8042.c, i8042-io.h)
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

#define I8042_DATA_REG 0x60
#define I8042_STATUS_REG 0x64
#define I8042_COMMAND_REG 0x64
#define I8042_STR_OBF 0x01
#define I8042_STR_IBF 0x02
#define I8042_CMD_CTL_RCTR 0x20
#define I8042_CMD_CTL_WCTR 0x60
#define I8042_CMD_PULSE_RESET 0xFE

static bool i8042_initialized = false;
static uintptr_t i8042_mmio_base = 0;

static inline uint8_t i8042_read_status(void) {
#if defined(__X86__)
   uint8_t ret;
   asm volatile("inb $0x64, %0" : "=a"(ret));
   return ret;
#else
   if (i8042_mmio_base) {
      return *(volatile uint8_t *)(i8042_mmio_base + 4);
   }// if
   return 0;
#endif
}// i8042_read_status

static inline uint8_t i8042_read_data(void) {
#if defined(__X86__)
   uint8_t ret;
   asm volatile("inb $0x60, %0" : "=a"(ret));
   return ret;
#else
   if (i8042_mmio_base) {
      return *(volatile uint8_t *)(i8042_mmio_base);
   }// if
   return 0;
#endif
}// i8042_read_data

int kbd_i8042_init(uintptr_t mmio_fallback) {
   infon("Probing i8042 PS/2 keyboard controller");
   i8042_mmio_base = mmio_fallback;

   /* Flush output buffer */
   while (i8042_read_status() & I8042_STR_OBF) {
      i8042_read_data();
   }// while

   i8042_initialized = true;
   successn("i8042: AT/PS2 keyboard controller online");
   return 0;
}// kbd_i8042_init

bool kbd_i8042_has_char(void) {
   if (!i8042_initialized) {
      return false;
   }// if
   return (i8042_read_status() & I8042_STR_OBF) != 0;
}// kbd_i8042_has_char

char kbd_i8042_get_scancode(void) {
   if (!kbd_i8042_has_char()) {
      return 0;
   }// if
   return (char)i8042_read_data();
}// kbd_i8042_get_scancode
