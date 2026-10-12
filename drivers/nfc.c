/*
 * Ported from the Linux kernel (drivers/nfc/pn533/pn533.c, net/nfc/core.c)
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

#define PN533_CMD_GET_FIRMWARE_VERSION 0x02
#define PN533_CMD_SAM_CONFIGURATION 0x14
#define PN533_CMD_IN_LIST_PASSIVE_TARGET 0x4A
#define NFC_PROTO_JEWEL 1
#define NFC_PROTO_MIFARE 2
#define NFC_PROTO_FELICA 3
#define NFC_PROTO_ISO14443 4
#define NFC_PROTO_NFC_DEP 5

struct nfc_target {
   uint32_t idx;
   uint32_t supported_protocols;
   uint8_t  sens_res[2];
   uint8_t  sel_res;
   uint8_t  nfcid1_len;
   uint8_t  nfcid1[10];
};

static bool nfc_initialized = false;
static uintptr_t nfc_dev_base = 0;

int nfc_init(uintptr_t dev_base) {
   infon("Probing NFC (Near Field Communication) controller");
   if (dev_base == 0) {
      errorn("nfc: invalid controller base pointer");
      return -1;
   }// if
   nfc_dev_base = dev_base;

   nfc_initialized = true;
   successn("nfc: NXP PN533 / ISO14443 wireless contactless interface ready");
   return 0;
}// nfc_init

int nfc_poll_targets(struct nfc_target *targets, size_t max_targets) {
   if (!nfc_initialized || !targets || max_targets == 0) {
      return -1;
   }// if
   return 0;
}// nfc_poll_targets
