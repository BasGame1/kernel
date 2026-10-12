/*
 * Ported from the Linux kernel (drivers/video/fbdev/goldfishfb.c)
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

#define FB_GET_WIDTH 0x00
#define FB_GET_HEIGHT 0x04
#define FB_INT_STATUS 0x08
#define FB_INT_ENABLE 0x0C
#define FB_SET_BASE 0x10
#define FB_SET_ROTATION 0x14
#define FB_SET_BLANK 0x18
#define FB_GET_PHYS_WIDTH 0x1C
#define FB_GET_PHYS_HEIGHT 0x20
#define FB_GET_FORMAT 0x24

static bool goldfish_fb_initialized = false;
static uint32_t goldfish_width = 320;
static uint32_t goldfish_height = 240;

int screen_goldfish_init(uintptr_t base_addr, uintptr_t fb_phys) {
   infon("Probing Android Goldfish Framebuffer display");
   if (base_addr == 0) {
      errorn("goldfish-fb: invalid base address");
      return -1;
   }// if

   goldfish_width = *(volatile uint32_t *)(base_addr + FB_GET_WIDTH);
   goldfish_height = *(volatile uint32_t *)(base_addr + FB_GET_HEIGHT);

   if (fb_phys != 0) {
      *(volatile uint32_t *)(base_addr + FB_SET_BASE) = (uint32_t)fb_phys;
   }// if

   goldfish_fb_initialized = true;
   successn("goldfish-fb: display controller online");
   return 0;
}// screen_goldfish_init

uint32_t screen_goldfish_get_width(void) {
   return goldfish_width;
}// screen_goldfish_get_width

uint32_t screen_goldfish_get_height(void) {
   return goldfish_height;
}// screen_goldfish_get_height
