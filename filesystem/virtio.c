#include <stdint.h>
#include <stdbool.h>

#include "../include/hardware.h"
#include "../include/kernel.h"

uint32_t probe_mmio() {
  infon("Probbing hardware addresses for block device");
  uint32_t i;
  bool is_modern;
  bool is_legacy;
  bool is_network;
  bool is_block;
  for (i = START_HARDWARE_ADDRESS; i < END_HARDWARE_ADDRESS; i += 0x1000) {
     is_modern = 0;
     is_legacy = 0;
     is_network = 0;
     is_block = 0;
     volatile uint32_t *magicvalue = ((volatile uint32_t *) i + (uintptr_t) VIRTIO_MAGIC_VALUE);
     volatile uint32_t *deviceid = ((volatile uint32_t *) i + (uintptr_t) VIRTIO_DEVICE_ID);
     volatile uint32_t *version = ((volatile uint32_t *) i + (uintptr_t) VIRTIO_VERSION);  
     if (*magicvalue != VIRTIO_MAGIC_EQUAL_TO) {
        infon("virtio magic not equal, skipping");
        continue;
     } else {
        infon("Virtio magic found");
     }// if to compare magic value
     if (*version == 1) {
        info("Address: "); 
        print_hex(i);
        infon(" is legacy");
        is_legacy = 1;
     } else if (*version == 2) {
        info("Address: ");
        print_hex(i);
        infon(" is modern");
        is_modern = 1;
     }// using 2 vars instead of 1 to not confunse myself in the future
     if (*deviceid == 1) {
        info("Address: ");
        print_hex(i);
        infon("Is not block device");
        is_network = 1;
     } else if (*deviceid == 2) {
        info("Address: ");
        print_hex(i);
        infon("Is block device");
        is_block = 1;
        break;
     }// same about 2 vars
  }// while
  if (!is_block) {
     return 0;
  } else {
     return i;
  }// if check if error 404 on the block device
}// probe_mmio
void init_fs() {
   uint32_t address = probe_mmio();
   if (address == 0) {
      fataln("No addresses found");
   }// if check if no addresses found
}// init_fs
