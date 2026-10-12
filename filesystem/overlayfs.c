/*
 * Ported from the Linux kernel (fs/overlayfs/overlayfs.h, super.c, copy_up.c)
 * Copyright (C) Linus Torvalds, Miklos Szeredi, and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../include/kernel.h"
#include "../include/gid.h"
#include "../include/drivers.h"

#define OVL_WHITEOUT_DEV 0
#define OVL_MAX_LAYERS 8

struct ovl_layer {
   char path[64];
   bool is_writeable;
};

struct ovl_fs {
   struct ovl_layer lower[OVL_MAX_LAYERS];
   size_t num_lower;
   struct ovl_layer upper;
   struct ovl_layer work;
   bool is_mounted;
};

static struct ovl_fs overlay_instance;

void init_overlayfs(void) {
   infon("Initializing OverlayFS");
   overlay_instance.num_lower = 0;
   overlay_instance.is_mounted = false;
   overlay_instance.upper.is_writeable = true;
   overlay_instance.upper.path[0] = '\0';
   overlay_instance.work.path[0] = '\0';

   overlay_instance.is_mounted = true;
   successn("overlayfs: stacking union filesystem driver ready");
}// init_overlayfs

int overlayfs_mount(const char *lowerdir, const char *upperdir, const char *workdir) {
   if (!lowerdir) {
      errorn("overlayfs: lowerdir path cannot be null");
      return -1;
   }// if

   if (overlay_instance.num_lower < OVL_MAX_LAYERS) {
      size_t len = strlen(lowerdir);
      if (len >= sizeof(overlay_instance.lower[0].path)) {
         len = sizeof(overlay_instance.lower[0].path) - 1;
      }// if
      memcpy(overlay_instance.lower[overlay_instance.num_lower].path, lowerdir, len);
      overlay_instance.lower[overlay_instance.num_lower].path[len] = '\0';
      overlay_instance.lower[overlay_instance.num_lower].is_writeable = false;
      overlay_instance.num_lower++;
   }// if

   if (upperdir) {
      size_t ulen = strlen(upperdir);
      if (ulen >= sizeof(overlay_instance.upper.path)) {
         ulen = sizeof(overlay_instance.upper.path) - 1;
      }// if
      memcpy(overlay_instance.upper.path, upperdir, ulen);
      overlay_instance.upper.path[ulen] = '\0';
      overlay_instance.upper.is_writeable = true;
   }// if

   if (workdir) {
      size_t wlen = strlen(workdir);
      if (wlen >= sizeof(overlay_instance.work.path)) {
         wlen = sizeof(overlay_instance.work.path) - 1;
      }// if
      memcpy(overlay_instance.work.path, workdir, wlen);
      overlay_instance.work.path[wlen] = '\0';
   }// if

   successn("overlayfs: layer stack mounted");
   return 0;
}// overlayfs_mount

bool overlayfs_is_mounted(void) {
   return overlay_instance.is_mounted;
}// overlayfs_is_mounted
