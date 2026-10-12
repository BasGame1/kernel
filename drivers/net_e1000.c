/*
 * Ported from the Linux kernel (drivers/net/ethernet/intel/e1000/e1000_main.c, e1000_hw.h)
 * Copyright (C) Linus Torvalds and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/drivers.h"

#define E1000_CTRL 0x00000
#define E1000_STATUS 0x00008
#define E1000_EERD 0x00014
#define E1000_ICR 0x000C0
#define E1000_IMS 0x000D0
#define E1000_RCTL 0x00100
#define E1000_TCTL 0x00400
#define E1000_RDBAL 0x02800
#define E1000_RDBAH 0x02804
#define E1000_RDLEN 0x02808
#define E1000_RDH 0x02810
#define E1000_RDT 0x02818
#define E1000_TDBAL 0x03800
#define E1000_TDBAH 0x03804
#define E1000_TDLEN 0x03808
#define E1000_TDH 0x03810
#define E1000_TDT 0x03818
#define E1000_RAL 0x05400
#define E1000_RAH 0x05404
#define E1000_RCTL_EN (1 << 1)
#define E1000_RCTL_SBP (1 << 2)
#define E1000_RCTL_UPE (1 << 3)
#define E1000_RCTL_MPE (1 << 4)
#define E1000_RCTL_BAM (1 << 15)
#define E1000_RCTL_BSIZE_2048 (0 << 16)
#define E1000_RCTL_SECRC (1 << 26)
#define E1000_TCTL_EN (1 << 1)
#define E1000_TCTL_PSP (1 << 3)
#define E1000_TCTL_CT_SHIFT 4
#define E1000_TCTL_COLD_SHIFT 12
#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 32

struct e1000_rx_desc {
   uint64_t buffer_addr;
   uint16_t length;
   uint16_t checksum;
   uint8_t  status;
   uint8_t  errors;
   uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
   uint64_t buffer_addr;
   uint16_t length;
   uint8_t  cso;
   uint8_t  cmd;
   uint8_t  status;
   uint8_t  css;
   uint16_t special;
} __attribute__((packed));

static struct e1000_rx_desc rx_descs[E1000_NUM_RX_DESC] __attribute__((aligned(16)));
static struct e1000_tx_desc tx_descs[E1000_NUM_TX_DESC] __attribute__((aligned(16)));
static uint8_t rx_buffers[E1000_NUM_RX_DESC][2048] __attribute__((aligned(16)));
static uint8_t tx_buffers[E1000_NUM_TX_DESC][2048] __attribute__((aligned(16)));

static uint32_t rx_cur = 0;
static uint32_t tx_cur = 0;
static bool e1000_initialized = false;

static inline void e1000_write(uintptr_t base, uint32_t reg, uint32_t val) {
   *(volatile uint32_t *)(base + reg) = val;
}// e1000_write

static inline uint32_t e1000_read(uintptr_t base, uint32_t reg) {
   return *(volatile uint32_t *)(base + reg);
}// e1000_read

int e1000_init(uintptr_t mmio_base, uint8_t mac_out[6]) {
   infon("Initializing Intel e1000 Ethernet driver");
   
   /* Read MAC from RAL/RAH registers */
   uint32_t ral = e1000_read(mmio_base, E1000_RAL);
   uint32_t rah = e1000_read(mmio_base, E1000_RAH);
   uint8_t mac[6];
   mac[0] = ral & 0xFF;
   mac[1] = (ral >> 8) & 0xFF;
   mac[2] = (ral >> 16) & 0xFF;
   mac[3] = (ral >> 24) & 0xFF;
   mac[4] = rah & 0xFF;
   mac[5] = (rah >> 8) & 0xFF;

   if (mac_out) {
      for (int i = 0; i < 6; i++) {
         mac_out[i] = mac[i];
      }// for
   }// if

   for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
      rx_descs[i].buffer_addr = (uintptr_t)rx_buffers[i];
      rx_descs[i].status = 0;
   }// for
   e1000_write(mmio_base, E1000_RDBAL, (uint32_t)(uintptr_t)rx_descs);
   e1000_write(mmio_base, E1000_RDBAH, 0);
   e1000_write(mmio_base, E1000_RDLEN, E1000_NUM_RX_DESC * sizeof(struct e1000_rx_desc));
   e1000_write(mmio_base, E1000_RDH, 0);
   e1000_write(mmio_base, E1000_RDT, E1000_NUM_RX_DESC - 1);
   e1000_write(mmio_base, E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_BSIZE_2048 | E1000_RCTL_SECRC);

   for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
      tx_descs[i].buffer_addr = (uintptr_t)tx_buffers[i];
      tx_descs[i].cmd = 0;
      tx_descs[i].status = 1;
   }// for
   e1000_write(mmio_base, E1000_TDBAL, (uint32_t)(uintptr_t)tx_descs);
   e1000_write(mmio_base, E1000_TDBAH, 0);
   e1000_write(mmio_base, E1000_TDLEN, E1000_NUM_TX_DESC * sizeof(struct e1000_tx_desc));
   e1000_write(mmio_base, E1000_TDH, 0);
   e1000_write(mmio_base, E1000_TDT, 0);
   e1000_write(mmio_base, E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | (15 << E1000_TCTL_CT_SHIFT) | (64 << E1000_TCTL_COLD_SHIFT));

   e1000_initialized = true;
   successn("e1000: Ethernet controller initialized successfully");
   return 0;
}// e1000_init

int e1000_send_packet(uintptr_t mmio_base, const void *packet, uint16_t length) {
   if (!e1000_initialized || length > 2048) {
      return -1;
   }// if
   tx_descs[tx_cur].buffer_addr = (uintptr_t)tx_buffers[tx_cur];
   tx_descs[tx_cur].length = length;
   tx_descs[tx_cur].cmd = (1 << 0) | (1 << 1) | (1 << 3);
   tx_descs[tx_cur].status = 0;

   const uint8_t *src = (const uint8_t *)packet;
   uint8_t *dst = tx_buffers[tx_cur];
   for (uint16_t i = 0; i < length; i++) {
      dst[i] = src[i];
   }// for

   uint32_t prev = tx_cur;
   tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;
   e1000_write(mmio_base, E1000_TDT, tx_cur);

   while (!(tx_descs[prev].status & 0xFF)) {
      //wait
   }// while
   return 0;
}// e1000_send_packet

int e1000_recv_packet(uintptr_t mmio_base, void *buffer, uint16_t max_len) {
   if (!e1000_initialized) {
      return -1;
   }// if
   if (!(rx_descs[rx_cur].status & 0x01)) {
      return 0;
   }// if

   uint16_t len = rx_descs[rx_cur].length;
   if (len > max_len) {
      len = max_len;
   }// if

   const uint8_t *src = rx_buffers[rx_cur];
   uint8_t *dst = (uint8_t *)buffer;
   for (uint16_t i = 0; i < len; i++) {
      dst[i] = src[i];
   }// for

   rx_descs[rx_cur].status = 0;
   uint32_t old_cur = rx_cur;
   rx_cur = (rx_cur + 1) % E1000_NUM_RX_DESC;
   e1000_write(mmio_base, E1000_RDT, old_cur);

   return len;
}// e1000_recv_packet
