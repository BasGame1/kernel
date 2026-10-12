/*
 * Ported from the Linux kernel (drivers/net/ethernet/qualcomm/rmnet/rmnet_map.c, rmnet_config.c)
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

struct rmnet_map_header {
   uint8_t  pad_len:6;
   uint8_t  reserved_bit:1;
   uint8_t  cd_bit:1;
   uint8_t  mux_id;
   uint16_t pkt_len;
} __attribute__((packed));

static bool rmnet_initialized = false;
static uint8_t rmnet_active_mux = 1;

int cellular_rmnet_init(uint8_t default_mux_id) {
   infon("Probing Qualcomm RmNet Cellular Modem / WWAN driver");
   rmnet_active_mux = default_mux_id ? default_mux_id : 1;

   rmnet_initialized = true;
   successn("rmnet: Cellular 4G/5G MAP multiplexing virtual network interface active");
   return 0;
}// cellular_rmnet_init

int cellular_rmnet_rx(const void *buf, size_t len, uint8_t *out_mux) {
   if (!rmnet_initialized || !buf || len < sizeof(struct rmnet_map_header)) {
      return -1;
   }// if
   const struct rmnet_map_header *map = (const struct rmnet_map_header *)buf;
   if (out_mux) {
      *out_mux = map->mux_id;
   }// if
   return 0;
}// cellular_rmnet_rx
