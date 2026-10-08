#ifndef __HARDWARE_H
#define __HARDWARE_H

#define START_HARDWARE_ADDRESS 0x10001000
#define END_HARDWARE_ADDRESS 0x10008000
#define SCREEN_ADDRESS 0x10000000
#define UART_STATUS 0x10000005

/* Virtio and fs */
#define VIRTIO_DEVICE_STATUS 0x070
#define VIRTIO_DEVICE_ID 0x008
#define VIRTIO_VERSION 0x004
#define VIRTIO_MAGIC_VALUE 0x000
// VIRTIO_MAGIC_VALUE should be equal to VIRTIO_MAGIC_EQUAL_TO
#define VIRTIO_MAGIC_EQUAL_TO 0x74726976
#define VIRTIO_DEVICE_FEATURES 0x010
#define VIRTIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_DRIVER_FEATURES 0x020
#define VIRTIO_DRIVER_FEATURES_SEL 0x024
#define VIRTQ_NUM 16   
#define VRING_DESC_F_NEXT 1
#define VRING_DESC_F_WRITE 2
#define VIRTIO_QUEUE_SEL 0x030
#define VIRTIO_QUEUE_NUM_MAX 0x034
#define VIRTIO_QUEUE_NUM 0x038
#define VIRTIO_QUEUE_READY 0x044
#define VIRTIO_QUEUE_DESC_LOW 0x080
#define VIRTIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_QUEUE_DRIVER_LOW 0x090
#define VIRTIO_QUEUE_DRIVER_HIGH 0x094
#define VIRTIO_QUEUE_DEVICE_LOW 0x0a0
#define VIRTIO_QUEUE_DEVICE_HIGH 0x0a4
#define VIRTIO_QUEUE_NOTIFY 0x050
#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1
struct virtq_desc {
   uint64_t addr;
   uint32_t len;
   uint16_t flags;
   uint16_t next;
};
struct virtq_avail {
   uint16_t flags;
   uint16_t idx;
   uint16_t ring[VIRTQ_NUM];
   uint16_t unused;
};
struct virtq_used_elem {
   uint32_t id;
   uint32_t len;
};    
struct virtq_used {
   uint16_t flags;
   uint16_t idx;
   struct virtq_used_elem ring[VIRTQ_NUM];
};
struct virtio_blk_req {
   uint32_t type;     // VIRTIO_BLK_T_IN or VIRTIO_BLK_T_OUT
   uint32_t reserved; // 0
   uint64_t sector;   // LBA sector number (0, 1, 2, ...)
};
#endif
