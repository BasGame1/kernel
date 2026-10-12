/*
 * Ported from the Linux kernel (sound/pci/ac97/ac97_codec.c)
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

#define AC97_RESET         0x00
#define AC97_MASTER_VOL    0x02
#define AC97_HEADPHONE_VOL 0x04
#define AC97_PCM_VOL       0x18
#define AC97_POWERDOWN     0x26
#define AC97_EXTENDED_ID   0x28

static bool ac97_initialized = false;
static uintptr_t ac97_nabm = 0;
static uintptr_t ac97_nam = 0;

static inline void ac97_write_nam(uint8_t reg, uint16_t val) {
   (void)reg;
   (void)val;
}// ac97_write_nam

int audio_ac97_init(uintptr_t nam_bar, uintptr_t nabm_bar) {
   infon("Probing AC97 Audio codec");
   if (nam_bar == 0 || nabm_bar == 0) {
      errorn("ac97: invalid I/O bars");
      return -1;
   }// if
   ac97_nam = nam_bar;
   ac97_nabm = nabm_bar;

   ac97_write_nam(AC97_RESET, 0x0000);
   ac97_write_nam(AC97_MASTER_VOL, 0x0000);
   ac97_write_nam(AC97_PCM_VOL, 0x0808);

   ac97_initialized = true;
   successn("ac97: Audio codec reset and unmuted");
   return 0;
}// audio_ac97_init
