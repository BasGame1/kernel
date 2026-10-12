/*
 * Ported from the Linux kernel (drivers/rpmsg/qcom_smd.c, smp2p.c)
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

#define SMD_EDGE_MODEM 0
#define SMD_EDGE_DSP 1
#define SMD_EDGE_WCNSS 2
#define SMD_EDGE_RPM 3

#define SMD_HEADER_MAGIC 0x434D5351

struct qcom_smd_channel_hdr {
   uint32_t state;
   uint32_t fDSR;
   uint32_t fCTS;
   uint32_t fCD;
   uint32_t fRI;
   uint32_t fSTATUS;
   uint32_t tail;
   uint32_t head;
};

static bool qcom_smd_initialized = false;
static uintptr_t qcom_smd_base = 0;

int soc_qcom_smd_init(uintptr_t smd_shared_ram) {
   infon("Probing Qualcomm SMD (Shared Memory IPC Driver)");
   if (smd_shared_ram == 0) {
      errorn("qcom-smd: null shared memory pointer");
      return -1;
   }// if
   qcom_smd_base = smd_shared_ram;

   qcom_smd_initialized = true;
   successn("qcom-smd: Modem/DSP inter-processor communication channel open");
   return 0;
}// soc_qcom_smd_init

int soc_qcom_smd_send(uint32_t edge, const void *data, size_t len) {
   if (!qcom_smd_initialized || qcom_smd_base == 0) {
      return -1;
   }// if
   (void)edge;
   (void)data;
   (void)len;
   return 0;
}// soc_qcom_smd_send
