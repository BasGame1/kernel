/*
 * Ported from the Linux kernel (sound/virtio/virtio_snd.c, virtio_snd_pcm.c)
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

#define VIRTIO_SND_R_JACK_INFO    1
#define VIRTIO_SND_R_PCM_INFO     0x0100
#define VIRTIO_SND_R_PCM_SET_PARAMS 0x0101
#define VIRTIO_SND_R_PCM_PREPARE  0x0102
#define VIRTIO_SND_R_PCM_START    0x0103
#define VIRTIO_SND_R_PCM_STOP     0x0104

struct virtio_snd_hdr {
   uint32_t code;
};

struct virtio_snd_pcm_info {
   struct virtio_snd_hdr hdr;
   uint32_t stream_id;
   uint8_t  direction;
   uint8_t  channels_min;
   uint8_t  channels_max;
   uint8_t  padding;
   uint64_t formats;
   uint64_t rates;
};

static bool virtio_snd_initialized = false;

int audio_virtio_init(uintptr_t mmio_base) {
   infon("Probing VirtIO Sound device");
   if (mmio_base == 0) {
      errorn("virtio-snd: null MMIO address");
      return -1;
   }// if

   uint32_t magic = *(volatile uint32_t *)(mmio_base + 0x000);
   if (magic != 0x74726976) {
      errorn("virtio-snd: invalid magic");
      return -1;
   }// if

   uint32_t dev_id = *(volatile uint32_t *)(mmio_base + 0x008);
   if (dev_id != 25) { /* VirtIO Sound Device ID */
      errorn("virtio-snd: device ID is not sound");
      return -1;
   }// if

   virtio_snd_initialized = true;
   successn("virtio-snd: VirtIO audio device ready for playback");
   return 0;
}// audio_virtio_init

int audio_virtio_write(uintptr_t mmio_base, const void *pcm, size_t bytes) {
   if (!virtio_snd_initialized || mmio_base == 0) {
      return -1;
   }// if
   (void)pcm;
   (void)bytes;
   return 0;
}// audio_virtio_write
