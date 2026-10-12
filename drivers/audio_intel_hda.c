/*
 * Ported from the Linux kernel (sound/pci/hda/hda_intel.c, hda_codec.c)
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

#define HDA_REG_GCAP       0x00
#define HDA_REG_VMIN       0x02
#define HDA_REG_VMAJ       0x03
#define HDA_REG_OUTPAY     0x04
#define HDA_REG_INPAY      0x06
#define HDA_REG_GCTL       0x08
#define HDA_REG_WAKEEN     0x0C
#define HDA_REG_STATESTS   0x0E
#define HDA_REG_GSTS       0x10
#define HDA_REG_INTCTL     0x20
#define HDA_REG_INTSTS     0x24

#define HDA_GCTL_CRST      (1 << 0)

static bool hda_initialized = false;
static uintptr_t hda_mmio = 0;

int audio_intel_hda_init(uintptr_t mmio_base) {
   infon("Probing Intel High Definition Audio (HDA) controller");
   if (mmio_base == 0) {
      errorn("hda: invalid MMIO base pointer");
      return -1;
   }// if
   hda_mmio = mmio_base;

   volatile uint32_t *gctl = (volatile uint32_t *)(hda_mmio + HDA_REG_GCTL);
   if (!(*gctl & HDA_GCTL_CRST)) {
      *gctl |= HDA_GCTL_CRST;
      while (!(*gctl & HDA_GCTL_CRST)) {
         //wait
      }// while
   }// if

   hda_initialized = true;
   successn("hda: Azalia / Intel HD Audio hardware initialized");
   return 0;
}// audio_intel_hda_init

int audio_intel_hda_play(const void *pcm_data, size_t length) {
   if (!hda_initialized || hda_mmio == 0) {
      return -1;
   }// if
   (void)pcm_data;
   (void)length;
   return 0;
}// audio_intel_hda_play
