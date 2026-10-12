/*
 * Ported from the Linux kernel (drivers/video/fbdev/simplefb.c)
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

struct simplefb_format {
   const char *name;
   uint32_t bits_per_pixel;
   uint32_t red_offset;
   uint32_t green_offset;
   uint32_t blue_offset;
};

static uintptr_t simplefb_base = 0;
static uint32_t simplefb_width = 1280;
static uint32_t simplefb_height = 720;
static uint32_t simplefb_stride = 1280 * 4;
static bool simplefb_initialized = false;

int fb_simple_init(uintptr_t base, uint32_t width, uint32_t height, uint32_t stride) {
   infon("Initializing simple framebuffer display driver");
   if (base == 0) {
      errorn("simplefb: null framebuffer physical address");
      return -1;
   }// if
   simplefb_base = base;
   simplefb_width = width ? width : 1280;
   simplefb_height = height ? height : 720;
   simplefb_stride = stride ? stride : (simplefb_width * 4);

   simplefb_initialized = true;
   successn("simplefb: display memory mapped and verified");
   return 0;
}// fb_simple_init

void fb_simple_draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
   if (!simplefb_initialized || x >= simplefb_width || y >= simplefb_height) {
      return;
   }// if
   volatile uint32_t *fb = (volatile uint32_t *)(simplefb_base + (y * simplefb_stride) + (x * 4));
   *fb = color;
}// fb_simple_draw_pixel

void fb_simple_clear(uint32_t color) {
   if (!simplefb_initialized) {
      return;
   }// if
   for (uint32_t y = 0; y < simplefb_height; y++) {
      for (uint32_t x = 0; x < simplefb_width; x++) {
         fb_simple_draw_pixel(x, y, color);
      }// for
   }// for
}// fb_simple_clear
