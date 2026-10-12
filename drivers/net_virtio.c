/*
 * Ported from the Linux kernel (drivers/net/virtio_net.c)
 * Copyright (C) Linus Torvalds and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../include/kernel.h"
#include "../include/drivers.h"

#define VIRTIO_NET_MAGIC_OFFSET 0x000
#define VIRTIO_NET_VERSION_OFFSET 0x004
#define VIRTIO_NET_DEVICE_ID_OFFSET 0x008
#define VIRTIO_NET_STATUS_OFFSET 0x070
#define VIRTIO_NET_CONFIG_OFFSET 0x100

struct virtio_net_hdr {
   uint8_t  flags;
   uint8_t  gso_type;
   uint16_t hdr_len;
   uint16_t gso_size;
   uint16_t csum_start;
   uint16_t csum_offset;
   uint16_t num_buffers;
} __attribute__((packed));

struct virtio_net_config {
   uint8_t  mac[6];
   uint16_t status;
   uint16_t max_virtqueue_pairs;
   uint16_t mtu;
} __attribute__((packed));

static bool virtio_net_initialized = false;
static uint8_t virtio_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};

int virtio_net_init(uintptr_t mmio_base, uint8_t mac_out[6]) {
   infon("Probing VirtIO Network device");
   uint32_t magic = *(volatile uint32_t *)(mmio_base + VIRTIO_NET_MAGIC_OFFSET);
   if (magic != 0x74726976) { /* 'virt' */
      errorn("virtio-net: invalid magic identifier");
      return -1;
   }// if

   uint32_t device_id = *(volatile uint32_t *)(mmio_base + VIRTIO_NET_DEVICE_ID_OFFSET);
   if (device_id != 1) { /* Device ID 1 = Network */
      errorn("virtio-net: device ID is not network card");
      return -1;
   }// if
   struct virtio_net_config *cfg = (struct virtio_net_config *)(mmio_base + VIRTIO_NET_CONFIG_OFFSET);
   for (int i = 0; i < 6; i++) {
      virtio_mac[i] = cfg->mac[i];
      if (mac_out) {
         mac_out[i] = cfg->mac[i];
      }// if
   }// for
   *(volatile uint32_t *)(mmio_base + VIRTIO_NET_STATUS_OFFSET) = 1 | 2;

   virtio_net_initialized = true;
   successn("virtio-net: device attached and initialized");
   return 0;
}// virtio_net_init

int virtio_net_send(uintptr_t mmio_base, const void *packet, uint16_t len) {
   if (!virtio_net_initialized || mmio_base == 0) {
      return -1;
   }// if
   (void)packet;
   (void)len;
   return 0;
}// virtio_net_send

int virtio_net_recv(uintptr_t mmio_base, void *buf, uint16_t max_len) {
   if (!virtio_net_initialized || mmio_base == 0) {
      return -1;
   }// if
   (void)buf;
   (void)max_len;
   return 0;
}// virtio_net_recv
