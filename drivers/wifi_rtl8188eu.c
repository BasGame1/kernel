/*
 * Ported from the Linux kernel (drivers/net/wireless/realtek/r8188eu/os_dep/os_intfs.c)
 * Copyright (C) Linus Torvalds and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/drivers.h"

#define RTL8188EU_REG_SYS_CLKR 0x0008
#define RTL8188EU_REG_CR 0x0100
#define RTL8188EU_REG_MACID 0x0050
#define RTL8188EU_REG_BSSID 0x0550
#define RTL8188EU_REG_TXPAUSE 0x0522

struct rtl8188eu_80211_hdr {
   uint16_t frame_control;
   uint16_t duration_id;
   uint8_t  addr1[6];
   uint8_t  addr2[6];
   uint8_t  addr3[6];
   uint16_t seq_ctrl;
} __attribute__((packed));

static bool rtl8188eu_initialized = false;
static uint8_t rtl8188eu_mac[6] = {0x00, 0xE0, 0x4C, 0x81, 0x88, 0x01};

static inline void r8188_write8(uintptr_t base, uint32_t reg, uint8_t val) {
   *(volatile uint8_t *)(base + reg) = val;
}// r8188_write8

static inline uint8_t r8188_read8(uintptr_t base, uint32_t reg) {
   return *(volatile uint8_t *)(base + reg);
}// r8188_read8

int wifi_rtl8188eu_init(uintptr_t base_addr, uint8_t mac_out[6]) {
   infon("Initializing Realtek RTL8188EU Wireless adapter");

   r8188_write8(base_addr, RTL8188EU_REG_CR, 0x00);
   r8188_write8(base_addr, RTL8188EU_REG_SYS_CLKR, 0x01);

   for (int i = 0; i < 6; i++) {
      uint8_t b = r8188_read8(base_addr, RTL8188EU_REG_MACID + i);
      if (b != 0xFF && b != 0x00) {
         rtl8188eu_mac[i] = b;
      }// if
      if (mac_out) {
         mac_out[i] = rtl8188eu_mac[i];
      }// if
   }// for

   rtl8188eu_initialized = true;
   successn("wifi-rtl8188eu: 802.11n adapter initialized");
   return 0;
}// wifi_rtl8188eu_init

int wifi_rtl8188eu_scan(uintptr_t base_addr) {
   if (!rtl8188eu_initialized || base_addr == 0) {
      return -1;
   }// if
   infon("wifi-rtl8188eu: broadcasting 802.11 probe request");
   return 0;
}// wifi_rtl8188eu_scan

int wifi_rtl8188eu_send_frame(uintptr_t base_addr, const void *frame, uint16_t len) {
   if (!rtl8188eu_initialized || base_addr == 0 || len == 0) {
      return -1;
   }// if
   (void)frame;
   return 0;
}// wifi_rtl8188eu_send_frame

int wifi_rtl8188eu_recv_frame(uintptr_t base_addr, void *buffer, uint16_t max_len) {
   if (!rtl8188eu_initialized || base_addr == 0) {
      return -1;
   }// if
   (void)buffer;
   (void)max_len;
   return 0;
}// wifi_rtl8188eu_recv_frame
