/*
 * Ported from the Linux kernel (drivers/net/ethernet/realtek/8139too.c)
 * Copyright (C) Linus Torvalds and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/drivers.h"

#define RTL8139_MAC0 0x00
#define RTL8139_MAR0 0x08
#define RTL8139_TX_STATUS0 0x10
#define RTL8139_TX_ADDR0 0x20
#define RTL8139_RX_BUF 0x30
#define RTL8139_CHIP_CMD 0x37
#define RTL8139_CAPR 0x38
#define RTL8139_CBR 0x3A
#define RTL8139_INTR_MASK 0x3C
#define RTL8139_INTR_STATUS 0x3E
#define RTL8139_TX_CONFIG 0x40
#define RTL8139_RX_CONFIG 0x44
#define CMD_RESET 0x10
#define CMD_RX_ENABLE 0x08
#define CMD_TX_ENABLE 0x04
#define RCR_AAP (1 << 0)
#define RCR_APM (1 << 1)
#define RCR_AM (1 << 2)
#define RCR_AB (1 << 3)
#define RCR_WRAP (1 << 7)

static uint8_t rx_ring[8192 + 16 + 1500] __attribute__((aligned(4)));
static uint8_t tx_buffers[4][2048] __attribute__((aligned(4)));
static uint8_t tx_cur = 0;
static uint16_t rx_offset = 0;
static bool rtl8139_initialized = false;

static inline void rtl_outb(uintptr_t base, uint32_t reg, uint8_t val) {
 *(volatile uint8_t *)(base + reg) = val;
}// rtl_outb

static inline void rtl_outw(uintptr_t base, uint32_t reg, uint16_t val) {
 *(volatile uint16_t *)(base + reg) = val;
}// rtl_outw

static inline void rtl_outl(uintptr_t base, uint32_t reg, uint32_t val) {
 *(volatile uint32_t *)(base + reg) = val;
}// rtl_outl

static inline uint8_t rtl_inb(uintptr_t base, uint32_t reg) {
 return *(volatile uint8_t *)(base + reg);
}// rtl_inb

static inline uint16_t rtl_inw(uintptr_t base, uint32_t reg) {
 return *(volatile uint16_t *)(base + reg);
}// rtl_inw

static inline uint32_t rtl_inl(uintptr_t base, uint32_t reg) {
 return *(volatile uint32_t *)(base + reg);
}// rtl_inl

int rtl8139_init(uintptr_t base_io, uint8_t mac_out[6]) {
 infon("Initializing Realtek RTL8139 Ethernet driver");

 rtl_outb(base_io, 0x52, 0x00);
 rtl_outb(base_io, RTL8139_CHIP_CMD, CMD_RESET);
 while ((rtl_inb(base_io, RTL8139_CHIP_CMD) & CMD_RESET) != 0) {
 }// while

 for (int i = 0; i < 6; i++) {
 uint8_t b = rtl_inb(base_io, RTL8139_MAC0 + i);
 if (mac_out) {
 mac_out[i] = b;
 }// if
 }// for

 rtl_outl(base_io, RTL8139_RX_BUF, (uint32_t)(uintptr_t)rx_ring);

 rtl_outw(base_io, RTL8139_INTR_MASK, 0x0005);

 rtl_outl(base_io, RTL8139_RX_CONFIG, RCR_AAP | RCR_APM | RCR_AM | RCR_AB | RCR_WRAP);

 rtl_outb(base_io, RTL8139_CHIP_CMD, CMD_RX_ENABLE | CMD_TX_ENABLE);

 rtl8139_initialized = true;
 successn("rtl8139: Ethernet adapter initialized");
 return 0;
}// rtl8139_init

int rtl8139_send_packet(uintptr_t base_io, const void *data, uint16_t len) {
 if (!rtl8139_initialized || len > 1792) {
 return -1;
 }// if
 uint8_t *dst = tx_buffers[tx_cur];
 const uint8_t *src = (const uint8_t *)data;
 for (uint16_t i = 0; i < len; i++) {
 dst[i] = src[i];
 }// for

 rtl_outl(base_io, RTL8139_TX_ADDR0 + (tx_cur * 4), (uint32_t)(uintptr_t)dst);
 rtl_outl(base_io, RTL8139_TX_STATUS0 + (tx_cur * 4), (uint32_t)len & 0x1FFF);
 tx_cur = (tx_cur + 1) % 4;
 return 0;
}// rtl8139_send_packet

int rtl8139_recv_packet(uintptr_t base_io, void *buffer, uint16_t max_len) {
 if (!rtl8139_initialized) {
 return -1;
 }// if
 if (rtl_inb(base_io, RTL8139_CHIP_CMD) & 0x01) {
 return 0; /* buffer empty */
 }// if

 uint8_t *rx_ptr = rx_ring + rx_offset;
 uint16_t pkt_len = *(uint16_t *)(rx_ptr + 2);
 if (pkt_len > 4) {
 pkt_len -= 4; /* strip CRC */
 }// if
 if (pkt_len > max_len) {
 pkt_len = max_len;
 }// if

 uint8_t *src = rx_ptr + 4;
 uint8_t *dst = (uint8_t *)buffer;
 for (uint16_t i = 0; i < pkt_len; i++) {
 dst[i] = src[i];
 }// for

 rx_offset = (rx_offset + pkt_len + 4 + 3) & ~3;
 rtl_outw(base_io, RTL8139_CAPR, rx_offset - 16);
 return pkt_len;
}// rtl8139_recv_packet
