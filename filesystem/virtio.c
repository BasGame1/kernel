#include <stdint.h>
#include <stdbool.h>

#include "../include/hardware.h"
#include "../include/kernel.h"
#include "../include/userspace.h"
#include "../include/gid.h"
#include "../include/ntar.h"

static uint32_t virtio_base = 0;
static struct virtq_desc queue_desc[VIRTQ_NUM] __attribute__((aligned(16)));
static struct virtq_avail queue_avail __attribute__((aligned(2)));
static struct virtq_used queue_used __attribute__((aligned(4)));
static uint16_t last_used_idx = 0;
static struct virtio_blk_req disk_req;
static volatile uint8_t disk_status;

/* Wrappers just for commodity */
int virtio_read_sector(uint64_t sector, void *buffer) {
   if (ntar_is_active()) {
      if (ntar_ram_read_sector(sector, buffer) == 0) {
         return 0;
      }// if
   }// if
   return virtio_blk_transfer(sector, buffer, false);
}
int virtio_write_sector(uint64_t sector, void *buffer) {
   if (ntar_is_active()) {
      return ntar_ram_write_sector(sector, buffer);
   }// if
   return virtio_blk_transfer(sector, buffer, true);
}

uint32_t probe_mmio() {
  printu("Probbing hardware addresses for block device");
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
     volatile uint32_t *magicvalue = ((volatile uint32_t *)(uintptr_t)(i +  VIRTIO_MAGIC_VALUE));
     volatile uint32_t *deviceid = ((volatile uint32_t *)(uintptr_t)(i + VIRTIO_DEVICE_ID));
     volatile uint32_t *version = ((volatile uint32_t *)(uintptr_t)(i + VIRTIO_VERSION));  
     if (*magicvalue != VIRTIO_MAGIC_EQUAL_TO) {
        infon("virtio magic not equal, skipping");
        continue;
     } else {
        infon("Virtio magic found");
     }// if to compare magic value
     if (*version == 1) {
        info("Address: "); 
        print_hex(i);
        printn(" is legacy");
        is_legacy = 1;
     } else if (*version == 2) {
        info("Address: ");
        print_hex(i);
        printn(" is modern");
        is_modern = 1;
     }// using 2 vars instead of 1 to not confunse myself in the future
     if (*deviceid == 1) {
        info("Address: ");
        print((char *)i);
        printn("Is not block device");
        is_network = 1;
     } else if (*deviceid == 2) {
        info("Address: ");
        print((char*)i);
        printn("Is block device");
        is_block = 1;
        break;
     }// same about 2 vars
  }// while
  if (!is_block) {
     return 0;
  } else {
     return i;
  }// if
}// probe_mmio

int virtio_blk_transfer(uint64_t sector, void *buffer, bool is_write) {
   if (virtio_base == 0) {
      errorn("virtio disk was not initialized");
      return -1;
   }// if
   disk_req.type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
   disk_req.reserved = 0;
   disk_req.sector = sector;
   disk_status = 0xFF;
   queue_desc[0].addr = (uint64_t)(uintptr_t)&disk_req;
   queue_desc[0].len = sizeof(struct virtio_blk_req);
   queue_desc[0].flags = VRING_DESC_F_NEXT;
   queue_desc[0].next = 1; 
   queue_desc[1].addr = (uint64_t)(uintptr_t)buffer;
   queue_desc[1].len = 512;
   queue_desc[1].flags = VRING_DESC_F_NEXT | (is_write ? 0 : VRING_DESC_F_WRITE);
   queue_desc[1].next = 2;   
   queue_desc[2].addr = (uint64_t)(uintptr_t)&disk_status;
   queue_desc[2].len = 1;
   queue_desc[2].flags = VRING_DESC_F_WRITE;
   queue_desc[2].next = 0;   
   uint16_t avail_idx = queue_avail.idx % VIRTQ_NUM;
   queue_avail.ring[avail_idx] = 0;   
   asm volatile("fence w,w" ::: "memory");
   queue_avail.idx++;
   asm volatile("fence w,w" ::: "memory");   
   *(volatile uint32_t *)(uintptr_t)(virtio_base + VIRTIO_QUEUE_NOTIFY) = 0;    
   while (queue_used.idx == last_used_idx) {
      asm volatile("nop");
   }// while
   last_used_idx = queue_used.idx;
   if (disk_status != 0) {
      errorn("Virtio disk transfer failed");
      return -1;
   }// if to check if transfer failed
   return 0;
}// virtio_blk_transfer

void init_fs() {
   uint32_t address = probe_mmio();
   if (address == 0) {
      fataln("no addresses found");
      return;
   }// if check if no addresses found
   virtio_base = address;
   volatile uint32_t *status = (volatile uint32_t *)(uintptr_t)(address + VIRTIO_DEVICE_STATUS);
   infon("Trying to update registry");
   *status = 0;
   *status = 1;
   *status = 1 | 2;
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_DEVICE_FEATURES_SEL) = 0;
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_DRIVER_FEATURES_SEL) = 0;
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_DRIVER_FEATURES) = 0;
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_DEVICE_FEATURES_SEL) = 1;
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_DRIVER_FEATURES_SEL) = 1;
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_DRIVER_FEATURES) = (1 << 0);
   *status = 1 | 2 | 8;
   if ((*status & 8) == 0 ) {
      fataln("Driver features were not accepted");
   }// if to see if the value is accepted
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_SEL) = 0;
   uint32_t max_queue = *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_NUM_MAX);
   if (max_queue == 0) {
      fataln("Queue not supported");
   }// if to see for support for queue
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_NUM) = VIRTQ_NUM;
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_DESC_LOW) = (uint32_t)(uintptr_t)&queue_desc[0];
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_DESC_HIGH) = (uint32_t)(((uint64_t)(uintptr_t)&queue_desc[0]) >> 32);
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_DRIVER_LOW) = (uint32_t)(uintptr_t)&queue_avail;
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_DRIVER_HIGH) = (uint32_t)(((uint64_t)(uintptr_t)&queue_avail) >> 32);
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_DEVICE_LOW) = (uint32_t)(uintptr_t)&queue_used;
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_DEVICE_HIGH) = (uint32_t)(((uint64_t)(uintptr_t)&queue_used) >> 32);
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_READY) = 1;
   *status = 1 | 2 | 8 | 4;
   successn("Virtio block device was successfull");
   #ifdef __DEBUG__
   // Test if sector is read
   static uint8_t sector_buffer[512];
   if (virtio_read_sector(0, sector_buffer) == 0) {
      successn("Sector 0 read successful");
   } else {
      errorn("Sector 0 read failed");
   }
   #endif
}// init_fs

void virtio_driver_process(void) {
   init_fs();
   while (1) {
      struct virtio_ipc_req req;
      uint32_t sender = user_ipc_recv(0, &req);
      if (sender != (uint32_t)-1) {
         int status = -1;
         if (req.cmd == 0) {
            status = virtio_read_sector(req.sector, req.buffer);
         } else if (req.cmd == 1) {
            status = virtio_write_sector(req.sector, req.buffer);
         }// if
         user_ipc_send(sender, (void *)(intptr_t)status);
      }// if
   }// while
}// virtio_driver_process
    
void create_virtio_process() {
   infon("Starting virtio process");
   uint32_t pid = create_process((void *)virtio_driver_process, VIRTIO_GID);
   struct process *virtio_process = get_process_by_pid(pid);
   if (virtio_process) {
      uint32_t gid = virtio_process->gid;
      uint32_t sid = generate_sid(gid, (uintptr_t)virtio_driver_process);
      virtio_process->sid = sid;
   }// if
   uint32_t i = START_HARDWARE_ADDRESS;
   while (i < END_HARDWARE_ADDRESS) {
      pkcall_map_device(i, i);
      i += 4096;
   }// while
   successn("Virtio process started");
}// create_virtio_process
