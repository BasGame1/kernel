/*
 * Ported from the Linux kernel (drivers/input/serio/ambakmi.c)
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

#define KMICR 0x00
#define KMISTAT 0x04
#define KMIDATA 0x08
#define KMICLKDIV 0x0C
#define KMIIR 0x10
#define KMICR_TYPE_MASK (1 << 5)
#define KMICR_EN (1 << 2)
#define KMICR_RXINTREN (1 << 4)
#define KMISTAT_RXFULL (1 << 4)
#define KMISTAT_TXEMPTY (1 << 6)

static bool pl050_initialized = false;
static uintptr_t pl050_base = 0;

int kbd_pl050_init(uintptr_t mmio_base) {
   infon("Probing ARM PrimeCell PL050 PS/2 keyboard controller");
   if (mmio_base == 0) {
      errorn("pl050: invalid MMIO base pointer");
      return -1;
   }// if
   pl050_base = mmio_base;

   *(volatile uint8_t *)(pl050_base + KMICLKDIV) = 3;
   *(volatile uint8_t *)(pl050_base + KMICR) = KMICR_EN | KMICR_RXINTREN;

   pl050_initialized = true;
   successn("pl050: ARM KMI keyboard interface initialized");
   return 0;
}// kbd_pl050_init

bool kbd_pl050_has_data(void) {
   if (!pl050_initialized || pl050_base == 0) {
      return false;
   }// if
   return (*(volatile uint8_t *)(pl050_base + KMISTAT) & KMISTAT_RXFULL) != 0;
}// kbd_pl050_has_data

char kbd_pl050_getc(void) {
   if (!kbd_pl050_has_data()) {
      return 0;
   }// if
   return (char)(*(volatile uint8_t *)(pl050_base + KMIDATA));
}// kbd_pl050_getc
