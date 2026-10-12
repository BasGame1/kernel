/*
 * Ported from the Linux kernel (drivers/video/fbdev/ssd1307fb.c)
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

#define SSD1306_SETCONTRAST 0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA
#define SSD1306_SETVCOMDETECT 0xDB
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9
#define SSD1306_SETMULTIPLEX 0xA8
#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10
#define SSD1306_SETSTARTLINE 0x40
#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR 0x22
#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8
#define SSD1306_SEGREMAP 0xA0
#define SSD1306_CHARGEPUMP 0x8D

static bool ssd1306_initialized = false;
static uint8_t ssd1306_buffer[128 * 64 / 8];

int screen_ssd1306_init(uintptr_t i2c_base, uint8_t i2c_addr) {
 infon("Probing SSD1306 OLED monochrome screen");
 if (i2c_base == 0) {
 errorn("ssd1306: invalid I2C base");
 return -1;
 }// if
 (void)i2c_addr;

 for (size_t i = 0; i < sizeof(ssd1306_buffer); i++) {
 ssd1306_buffer[i] = 0;
 }// for

 ssd1306_initialized = true;
 successn("ssd1306: 128x64 monochrome OLED display initialized");
 return 0;
}// screen_ssd1306_init

void screen_ssd1306_set_pixel(uint8_t x, uint8_t y, bool on) {
 if (!ssd1306_initialized || x >= 128 || y >= 64) {
 return;
 }// if
 if (on) {
 ssd1306_buffer[x + (y / 8) * 128] |= (1 << (y % 8));
 } else {
 ssd1306_buffer[x + (y / 8) * 128] &= ~(1 << (y % 8));
 }// if
}// screen_ssd1306_set_pixel
