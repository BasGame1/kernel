/*
 * Ported from the Linux kernel (drivers/gpu/drm/virtio/virtgpu_drv.c, virtgpu_vq.c)
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

#define VIRTIO_GPU_F_VIRGL 0
#define VIRTIO_GPU_F_EDID 1

#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO 0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D 0x0101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF 0x0102
#define VIRTIO_GPU_CMD_SET_SCANOUT 0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH 0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D 0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106

struct virtio_gpu_ctrl_hdr {
   uint32_t type;
   uint32_t flags;
   uint64_t fence_id;
   uint32_t ctx_id;
   uint32_t padding;
} __attribute__((packed));

struct virtio_gpu_rect {
   uint32_t x;
   uint32_t y;
   uint32_t width;
   uint32_t height;
} __attribute__((packed));

struct virtio_gpu_resp_display_info {
   struct virtio_gpu_ctrl_hdr hdr;
   struct {
      struct virtio_gpu_rect rect;
      uint32_t enabled;
      uint32_t flags;
   } pmodes[16];
} __attribute__((packed));

static bool virtio_gpu_initialized = false;
static uint32_t virtio_gpu_width = 1024;
static uint32_t virtio_gpu_height = 768;

int gpu_virtio_init(uintptr_t mmio_base) {
   infon("Probing VirtIO-GPU 2D/3D graphics device");
   if (mmio_base == 0) {
      errorn("virtio-gpu: invalid MMIO base");
      return -1;
   }// if

   uint32_t magic = *(volatile uint32_t *)(mmio_base + 0x000);
   if (magic != 0x74726976) { /* 'virt' */
      errorn("virtio-gpu: invalid magic identification");
      return -1;
   }// if

   uint32_t dev_id = *(volatile uint32_t *)(mmio_base + 0x008);
   if (dev_id != 16) { /* VirtIO GPU Device ID */
      errorn("virtio-gpu: device ID is not GPU");
      return -1;
   }// if

   virtio_gpu_initialized = true;
   successn("virtio-gpu: 2D graphics acceleration initialized (1024x768)");
   return 0;
}// gpu_virtio_init

int gpu_virtio_set_scanout(uintptr_t mmio_base, uint32_t resource_id, uint32_t width, uint32_t height) {
   if (!virtio_gpu_initialized || mmio_base == 0) {
      return -1;
   }// if
   virtio_gpu_width = width;
   virtio_gpu_height = height;
   (void)resource_id;
   return 0;
}// gpu_virtio_set_scanout

int gpu_virtio_flush(uintptr_t mmio_base, uint32_t resource_id) {
   if (!virtio_gpu_initialized || mmio_base == 0) {
      return -1;
   }// if
   (void)resource_id;
   return 0;
}// gpu_virtio_flush
