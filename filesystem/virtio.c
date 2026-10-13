#include <stdint.h>
#include <stdbool.h>

#include "../include/hardware.h"
#include "../include/kernel.h"
#include "../include/userspace.h"
#include "../include/gid.h"
#include "../include/ntar.h"
#include "../include/memory.h"

#define VIRTIO_TIMEOUT_CYCLES 50000000
#define VIRTIO_BLK_S_OK 0

#if defined(__RISCV__) || defined(__riscv)
#define virtio_mb() asm volatile("fence rw,rw" ::: "memory")
#define virtio_rmb() asm volatile("fence r,r" ::: "memory")
#define virtio_wmb() asm volatile("fence w,w" ::: "memory")
#else
#define virtio_mb() asm volatile("" ::: "memory")
#define virtio_rmb() asm volatile("" ::: "memory")
#define virtio_wmb() asm volatile("" ::: "memory")
#endif

static uint32_t virtio_base = 0;
static uint32_t active_queue_size = VIRTQ_NUM;
static uint64_t disk_capacity_sectors = 0;
static volatile bool virtio_locked = false;
static volatile struct virtq_desc queue_desc[VIRTQ_NUM] __attribute__((aligned(16)));
static volatile struct virtq_avail queue_avail __attribute__((aligned(2)));
static volatile struct virtq_used queue_used __attribute__((aligned(4)));
static uint16_t last_used_idx = 0;

static struct virtio_blk_req disk_req[VIRTQ_NUM] __attribute__((aligned(16)));
static volatile uint32_t disk_status[VIRTQ_NUM] __attribute__((aligned(4)));

static uint16_t desc_free_head = 0;
static uint16_t desc_free_count = VIRTQ_NUM;
static uint16_t desc_next_free[VIRTQ_NUM];
static bool desc_in_use[VIRTQ_NUM];

static void init_desc_pool(uint16_t num) {
   if (num > VIRTQ_NUM) {
      num = VIRTQ_NUM;
   }// if
   for (uint16_t i = 0; i < VIRTQ_NUM; i++) {
      desc_next_free[i] = i + 1;
      desc_in_use[i] = false;
      queue_desc[i].addr = 0;
      queue_desc[i].len = 0;
      queue_desc[i].flags = 0;
      queue_desc[i].next = 0;
   }// for
   desc_free_head = 0;
   desc_free_count = num;
}// init_desc_pool

static int alloc_desc(void) {
   if (desc_free_count == 0) {
      return -1;
   }// if
   if (desc_free_head >= active_queue_size || desc_free_head >= VIRTQ_NUM) {
      return -1;
   }// if
   uint16_t d = desc_free_head;
   if (desc_in_use[d]) {
      return -1;
   }// if
   desc_free_head = desc_next_free[d];
   desc_free_count--;
   desc_in_use[d] = true;
   return (int)d;
}// alloc_desc

static void free_desc(uint16_t d) {
   if (d >= active_queue_size || d >= VIRTQ_NUM) {
      return;
   }// if
   if (!desc_in_use[d]) {
      return;
   }// if
   if (desc_free_count >= active_queue_size) {
      return;
   }// if
   queue_desc[d].addr = 0;
   queue_desc[d].len = 0;
   queue_desc[d].flags = 0;
   queue_desc[d].next = 0;
   desc_in_use[d] = false;
   desc_next_free[d] = desc_free_head;
   desc_free_head = d;
   desc_free_count++;
}// free_desc

int virtio_read_sector(uint64_t sector, void *buffer) {
   if (!buffer) {
      errorn("virtio: invalid read buffer");
      return -1;
   }// if
   if (ntar_is_active()) {
      if (ntar_ram_read_sector(sector, buffer) == 0) {
         return 0;
      }// if
   }// if
   return virtio_blk_transfer(sector, buffer, false);
}// virtio_read_sector

int virtio_write_sector(uint64_t sector, void *buffer) {
   if (!buffer) {
      errorn("virtio: invalid write buffer");
      return -1;
   }// if
   if (ntar_is_active()) {
      return ntar_ram_write_sector(sector, buffer);
   }// if
   return virtio_blk_transfer(sector, buffer, true);
}// virtio_write_sector

uint32_t probe_mmio(void) {
   infon("Probing hardware addresses for block device");
   uint32_t i;
   bool is_block = false;

   for (i = START_HARDWARE_ADDRESS; i < END_HARDWARE_ADDRESS; i += 0x1000) {
      volatile uint32_t *magicvalue = (volatile uint32_t *)(uintptr_t)(i + VIRTIO_MAGIC_VALUE);
      volatile uint32_t *deviceid = (volatile uint32_t *)(uintptr_t)(i + VIRTIO_DEVICE_ID);
      volatile uint32_t *version = (volatile uint32_t *)(uintptr_t)(i + VIRTIO_VERSION);

      if (*magicvalue != VIRTIO_MAGIC_EQUAL_TO) {
         continue;
      }// if

      if (*version != 2) {
         info("VirtIO at address ");
         print_hex(i);
         printn(" is legacy (v1), requiring modern (v2)");
         continue;
      }// if

      if (*deviceid == 1) {
         info("Address: ");
         print_hex(i);
         printn(" is network device");
      } else if (*deviceid == 2) {
         info("Address: ");
         print_hex(i);
         printn(" is block device");
         is_block = true;
         break;
      }// if
   }// for

   return is_block ? i : 0;
}// probe_mmio

int virtio_blk_transfer(uint64_t sector, void *buffer, bool is_write) {
   if (virtio_base == 0) {
      errorn("virtio: disk not initialized");
      return -1;
   }// if
   if (!buffer) {
      errorn("virtio: invalid buffer");
      return -1;
   }// if
   uintptr_t buf_addr = (uintptr_t)buffer;
   if (buf_addr < START_MEMORY || buf_addr > (UINTPTR_MAX - 512) || (buf_addr + 512) > END_MEMORY) {
      errorn("virtio: buffer out of memory bounds");
      return -1;
   }// if
   if (sector > (UINT64_MAX / 512)) {
      errorn("virtio: sector integer overflow");
      return -1;
   }// if
   if (disk_capacity_sectors > 0 && sector >= disk_capacity_sectors) {
      errorn("virtio: sector exceeds device capacity");
      return -1;
   }// if
   if (active_queue_size < 3 || active_queue_size > VIRTQ_NUM) {
      errorn("virtio: invalid queue size");
      return -1;
   }// if
   if (is_write && ntar_is_active()) {
      return ntar_ram_write_sector(sector, buffer);
   }// if
   if (virtio_locked) {
      errorn("virtio: transfer already in flight");
      return -1;
   }// if
   virtio_locked = true;

   int d0 = alloc_desc();
   int d1 = alloc_desc();
   int d2 = alloc_desc();
   if (d0 < 0 || d1 < 0 || d2 < 0 || d0 == d1 || d1 == d2 || d0 == d2) {
      if (d0 >= 0) {
         free_desc((uint16_t)d0);
      }// if
      if (d1 >= 0) {
         free_desc((uint16_t)d1);
      }// if
      if (d2 >= 0) {
         free_desc((uint16_t)d2);
      }// if
      virtio_locked = false;
      errorn("virtio: failed to allocate descriptor chain");
      return -1;
   }// if

   disk_req[d0].type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
   disk_req[d0].reserved = 0;
   disk_req[d0].sector = sector;
   disk_status[d0] = 0xFF;

   queue_desc[d0].addr = (uint64_t)(uintptr_t)&disk_req[d0];
   queue_desc[d0].len = sizeof(struct virtio_blk_req);
   queue_desc[d0].flags = VRING_DESC_F_NEXT;
   queue_desc[d0].next = (uint16_t)d1;

   queue_desc[d1].addr = (uint64_t)(uintptr_t)buffer;
   queue_desc[d1].len = 512;
   queue_desc[d1].flags = VRING_DESC_F_NEXT | (is_write ? 0 : VRING_DESC_F_WRITE);
   queue_desc[d1].next = (uint16_t)d2;

   queue_desc[d2].addr = (uint64_t)(uintptr_t)&disk_status[d0];
   queue_desc[d2].len = 1;
   queue_desc[d2].flags = VRING_DESC_F_WRITE;
   queue_desc[d2].next = 0;

   virtio_wmb();

   uint16_t avail_idx = queue_avail.idx % active_queue_size;
   queue_avail.ring[avail_idx] = (uint16_t)d0;
   virtio_wmb();
   queue_avail.idx++;
   virtio_wmb();

   *(volatile uint32_t *)(uintptr_t)(virtio_base + VIRTIO_QUEUE_NOTIFY) = 0;

   uint32_t timeout = VIRTIO_TIMEOUT_CYCLES;
   while ((*(volatile uint16_t *)&queue_used.idx) == last_used_idx) {
      // wait
      if (--timeout == 0) {
         errorn("virtio: transfer timed out, resetting device");
         init_fs();
         virtio_locked = false;
         return -1;
      }// if
      asm volatile("nop");
   }// while

   virtio_rmb();

   uint16_t used_idx = last_used_idx % active_queue_size;
   uint32_t used_id = queue_used.ring[used_idx].id;
   uint32_t used_len = queue_used.ring[used_idx].len;
   last_used_idx++;

   uint8_t status = *(volatile uint8_t *)&disk_status[d0];
   free_desc((uint16_t)d0);
   free_desc((uint16_t)d1);
   free_desc((uint16_t)d2);
   virtio_locked = false;

   if (used_id != (uint32_t)d0) {
      errorn("virtio: host returned mismatched descriptor id");
      init_fs();
      return -1;
   }// if
   if (is_write && used_len > 1) {
      errorn("virtio: host wrote to read-only buffer");
      init_fs();
      return -1;
   }// if
   if (!is_write && used_len > 513) {
      errorn("virtio: host response length out of bounds");
      init_fs();
      return -1;
   }// if
   if (status != VIRTIO_BLK_S_OK) {
      errorn("virtio: disk transfer failed with device status error");
      return -1;
   }// if

   return VIRTIO_BLK_S_OK;
}// virtio_blk_transfer

void init_fs(void) {
   uint32_t address = probe_mmio();
   if (address == 0 || address < START_HARDWARE_ADDRESS || address >= END_HARDWARE_ADDRESS) {
      fataln("virtio: no supported block device found");
      virtio_base = 0;
      return;
   }// if
   virtio_base = address;

   volatile uint32_t *status = (volatile uint32_t *)(uintptr_t)(address + VIRTIO_DEVICE_STATUS);
   infon("virtio: resetting device");
   *status = 0;
   virtio_mb();
   uint32_t rst_timeout = VIRTIO_TIMEOUT_CYCLES;
   while (*status != 0 && --rst_timeout > 0) {
      // wait
   }// while
   if (*status != 0) {
      fataln("virtio: device reset failed");
      virtio_base = 0;
      return;
   }// if

   *status = 1;
   virtio_mb();
   *status = 1 | 2;
   virtio_mb();

   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_DEVICE_FEATURES_SEL) = 0;
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_DRIVER_FEATURES_SEL) = 0;
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_DRIVER_FEATURES) = 0;

   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_DEVICE_FEATURES_SEL) = 1;
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_DRIVER_FEATURES_SEL) = 1;
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_DRIVER_FEATURES) = (1 << 0);
   virtio_wmb();

   *status = 1 | 2 | 8;
   virtio_mb();
   if ((*status & 8) == 0) {
      fataln("virtio: driver features not accepted");
      virtio_base = 0;
      return;
   }// if

   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_SEL) = 0;
   uint32_t max_queue = *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_NUM_MAX);
   if (max_queue == 0 || max_queue < 3) {
      fataln("virtio: queue size unsupported");
      virtio_base = 0;
      return;
   }// if

   uint32_t queue_num = VIRTQ_NUM;
   if (queue_num > max_queue) {
      queue_num = max_queue;
   }// if
   active_queue_size = queue_num;

   last_used_idx = 0;
   queue_avail.idx = 0;
   queue_avail.flags = 0;
   queue_used.idx = 0;
   queue_used.flags = 0;
   memset((void *)queue_desc, 0, sizeof(queue_desc));
   memset((void *)queue_avail.ring, 0, sizeof(queue_avail.ring));
   memset((void *)queue_used.ring, 0, sizeof(queue_used.ring));

   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_NUM) = queue_num;
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_DESC_LOW) = (uint32_t)(uintptr_t)&queue_desc[0];
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_DESC_HIGH) = (uint32_t)(((uint64_t)(uintptr_t)&queue_desc[0]) >> 32);
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_DRIVER_LOW) = (uint32_t)(uintptr_t)&queue_avail;
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_DRIVER_HIGH) = (uint32_t)(((uint64_t)(uintptr_t)&queue_avail) >> 32);
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_DEVICE_LOW) = (uint32_t)(uintptr_t)&queue_used;
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_DEVICE_HIGH) = (uint32_t)(((uint64_t)(uintptr_t)&queue_used) >> 32);
   *(volatile uint32_t *)(uintptr_t)(address + VIRTIO_QUEUE_READY) = 1;
   virtio_wmb();

   *status = 1 | 2 | 8 | 4;
   virtio_mb();
   if ((*status & 4) == 0) {
      fataln("virtio: device driver ok rejected");
      virtio_base = 0;
      return;
   }// if

   uint32_t cap_low = *(volatile uint32_t *)(uintptr_t)(address + 0x100);
   uint32_t cap_high = *(volatile uint32_t *)(uintptr_t)(address + 0x104);
   disk_capacity_sectors = ((uint64_t)cap_high << 32) | (uint64_t)cap_low;

   init_desc_pool((uint16_t)queue_num);
   virtio_locked = false;
   successn("virtio: block device initialized successfully");
#ifdef __DEBUG__
   static uint8_t sector_buffer[512];
   if (virtio_read_sector(0, sector_buffer) == 0) {
      successn("Sector 0 read successful");
   } else {
      errorn("Sector 0 read failed");
   }// if
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
      } else {
         user_yield();
      }// if
   }// while
}// virtio_driver_process

void create_virtio_process(void) {
   infon("Starting virtio process");
   uint32_t pid = create_process((void *)virtio_driver_process, VIRTIO_GID);
   if (pid == (uint32_t)-1) {
      errorn("virtio: failed to create driver process");
      return;
   }// if
   struct process *virtio_process = get_process_by_pid(pid);
   if (virtio_process) {
      uint32_t gid = virtio_process->gid;
      uint32_t sid = generate_sid(gid, (uintptr_t)virtio_driver_process);
      virtio_process->sid = sid;
   }// if
   uint32_t i = START_HARDWARE_ADDRESS;
   while (i < END_HARDWARE_ADDRESS) {
      pkcall_map_device(i, i);
      if (i > UINT32_MAX - 4096) {
         break;
      }// if
      i += 4096;
   }// while
   successn("virtio process started");
}// create_virtio_process
