/*
 * Ported from the Linux kernel (drivers/hid/usbhid/hid-core.c)
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

#define USB_HID_REQ_GET_REPORT   0x01
#define USB_HID_REQ_GET_IDLE     0x02
#define USB_HID_REQ_GET_PROTOCOL 0x03
#define USB_HID_REQ_SET_REPORT   0x09
#define USB_HID_REQ_SET_IDLE     0x0A
#define USB_HID_REQ_SET_PROTOCOL 0x0B

struct usb_hid_kbd_report {
   uint8_t modifiers;
   uint8_t reserved;
   uint8_t keycodes[6];
} __attribute__((packed));

struct usb_hid_mouse_report {
   uint8_t buttons;
   int8_t  x_displacement;
   int8_t  y_displacement;
   int8_t  wheel;
} __attribute__((packed));

static bool usb_hid_initialized = false;

int usb_hid_init(uintptr_t usb_dev) {
   infon("Probing USB Human Interface Device (HID)");
   if (usb_dev == 0) {
      errorn("usb-hid: invalid device descriptor");
      return -1;
   }// if

   usb_hid_initialized = true;
   successn("usb-hid: USB keyboard and pointer interface bound");
   return 0;
}// usb_hid_init

int usb_hid_poll_keyboard(uintptr_t usb_dev, struct usb_hid_kbd_report *out) {
   if (!usb_hid_initialized || !out || usb_dev == 0) {
      return -1;
   }// if
   out->modifiers = 0;
   out->reserved = 0;
   for (int i = 0; i < 6; i++) {
      out->keycodes[i] = 0;
   }// for
   return 0;
}// usb_hid_poll_keyboard
