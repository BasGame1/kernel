/*
 * Ported from the Linux kernel (drivers/gpu/drm/bochs/bochs_drv.c, bochs_hw.c)
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

#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF

#define VBE_DISPI_INDEX_ID 0x0
#define VBE_DISPI_INDEX_XRES 0x1
#define VBE_DISPI_INDEX_YRES 0x2
#define VBE_DISPI_INDEX_BPP 0x3
#define VBE_DISPI_INDEX_ENABLE 0x4
#define VBE_DISPI_INDEX_BANK 0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH 0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 0x7
#define VBE_DISPI_INDEX_X_OFFSET 0x8
#define VBE_DISPI_INDEX_Y_OFFSET 0x9

#define VBE_DISPI_DISABLED 0x00
#define VBE_DISPI_ENABLED 0x01
#define VBE_DISPI_LFB_ENABLED 0x40
#define VBE_DISPI_NOCLEARMEM 0x80

static bool bochs_vga_initialized = false;
static uint32_t bochs_xres = 800;
static uint32_t bochs_yres = 600;
static uint32_t bochs_bpp = 32;

static inline void bochs_vbe_write(uint16_t index, uint16_t data) {
   (void)index;
   (void)data;
}// bochs_vbe_write

int gpu_bochs_init(uintptr_t bar_base, uint32_t width, uint32_t height, uint32_t bpp) {
   infon("Probing Bochs/QEMU standard VGA display adapter");
   if (bar_base == 0) {
      errorn("bochs-vga: invalid framebuffer BAR address");
      return -1;
   }// if
   bochs_xres = width ? width : 800;
   bochs_yres = height ? height : 600;
   bochs_bpp = bpp ? bpp : 32;

   bochs_vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
   bochs_vbe_write(VBE_DISPI_INDEX_XRES, (uint16_t)bochs_xres);
   bochs_vbe_write(VBE_DISPI_INDEX_YRES, (uint16_t)bochs_yres);
   bochs_vbe_write(VBE_DISPI_INDEX_BPP, (uint16_t)bochs_bpp);
   bochs_vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

   bochs_vga_initialized = true;
   successn("bochs-vga: linear framebuffer display initialized");
   return 0;
}// gpu_bochs_init

bool gpu_bochs_is_active(void) {
   return bochs_vga_initialized;
}// gpu_bochs_is_active
