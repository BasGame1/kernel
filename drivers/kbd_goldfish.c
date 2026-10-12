/*
 * Ported from the Linux kernel (drivers/input/keyboard/goldfish_events.c)
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

#define GFX_REG_READ 0x00
#define GFX_REG_SET_PAGE 0x00
#define GFX_REG_LEN 0x04
#define GFX_REG_DATA 0x08

#define EV_KEY 0x01
#define EV_REL 0x02
#define EV_ABS 0x03

struct goldfish_event {
   uint32_t type;
   uint32_t code;
   uint32_t value;
};

static bool goldfish_kbd_initialized = false;
static uintptr_t goldfish_kbd_base = 0;

int kbd_goldfish_init(uintptr_t mmio_base) {
   infon("Probing Goldfish Virtual Event keyboard controller");
   if (mmio_base == 0) {
      errorn("goldfish-kbd: invalid MMIO base");
      return -1;
   }// if
   goldfish_kbd_base = mmio_base;

   goldfish_kbd_initialized = true;
   successn("goldfish-kbd: Event input controller operational");
   return 0;
}// kbd_goldfish_init

bool kbd_goldfish_poll_event(struct goldfish_event *ev) {
   if (!goldfish_kbd_initialized || !ev || goldfish_kbd_base == 0) {
      return false;
   }// if
   uint32_t type = *(volatile uint32_t *)(goldfish_kbd_base + GFX_REG_READ);
   if (type == 0) {
      return false;
   }// if
   ev->type = type;
   ev->code = *(volatile uint32_t *)(goldfish_kbd_base + GFX_REG_READ);
   ev->value = *(volatile uint32_t *)(goldfish_kbd_base + GFX_REG_READ);
   return true;
}// kbd_goldfish_poll_event
