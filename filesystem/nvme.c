#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../include/kernel.h"
#include "../include/drivers.h"

#define NVME_REG_CAP 0x0000
#define NVME_REG_VS 0x0008
#define NVME_REG_INTMS 0x000C
#define NVME_REG_INTMC 0x0010
#define NVME_REG_CC 0x0014
#define NVME_REG_CSTS 0x001C
#define NVME_REG_AQA 0x0024
#define NVME_REG_ASQ 0x0028
#define NVME_REG_ACQ 0x0030

#define NVME_CC_ENABLE (1 << 0)
#define NVME_CC_CSS_NVM (0 << 4)
#define NVME_CC_MPS_4K (0 << 7)
#define NVME_CC_IOSQES_64 (6 << 16)
#define NVME_CC_IOCQES_16 (4 << 20)
#define NVME_CSTS_RDY (1 << 0)
#define NVME_CSTS_CFS (1 << 1)

#define NVME_ADMIN_OP_CREATE_SQ 0x01
#define NVME_ADMIN_OP_CREATE_CQ 0x05
#define NVME_ADMIN_OP_IDENTIFY 0x06

#define NVME_NVM_OP_FLUSH 0x00
#define NVME_NVM_OP_WRITE 0x01
#define NVME_NVM_OP_READ 0x02

#define ADMIN_QUEUE_SIZE 16
#define IO_QUEUE_SIZE 64
#define NVME_TIMEOUT_CYCLES 10000000
#define NVME_SECTOR_SIZE 512
#define NVME_PAGE_SIZE 4096
#define NVME_MAX_PRP_LIST_ENTRIES 512
#define NVME_MAX_TRANSFER_BLOCKS 2048

struct nvme_command {
   uint8_t opcode;
   uint8_t flags;
   uint16_t command_id;
   uint32_t nsid;
   uint64_t reserved;
   uint64_t metadata;
   uint64_t prp1;
   uint64_t prp2;
   uint32_t cdw10;
   uint32_t cdw11;
   uint32_t cdw12;
   uint32_t cdw13;
   uint32_t cdw14;
   uint32_t cdw15;
} __attribute__((packed));

struct nvme_completion {
   uint32_t result;
   uint32_t reserved;
   uint16_t sq_head;
   uint16_t sq_id;
   uint16_t command_id;
   uint16_t status;
} __attribute__((packed));

static struct nvme_command admin_sq[ADMIN_QUEUE_SIZE] __attribute__((aligned(4096)));
static struct nvme_completion admin_cq[ADMIN_QUEUE_SIZE] __attribute__((aligned(4096)));
static struct nvme_command io_sq[IO_QUEUE_SIZE] __attribute__((aligned(4096)));
static struct nvme_completion io_cq[IO_QUEUE_SIZE] __attribute__((aligned(4096)));
static uint64_t prp_list[NVME_MAX_PRP_LIST_ENTRIES] __attribute__((aligned(4096)));

static uint16_t admin_sq_tail = 0;
static uint16_t admin_cq_head = 0;
static uint8_t admin_cq_phase = 1;

static uint16_t io_sq_tail = 0;
static uint16_t io_cq_head = 0;
static uint8_t io_cq_phase = 1;
static uint16_t next_cid = 1;

static uint32_t db_stride = 4;
static uintptr_t active_bar_base = 0;
static bool nvme_initialized = false;
static volatile bool nvme_locked = false;

static inline void nvme_barrier(void) {
   #ifdef __X86__
      asm volatile("mfence" ::: "memory");
   #else
      #ifdef __RISCV__
         asm volatile("fence iorw,iorw" ::: "memory");
      #else
         #ifdef __ARM__
            asm volatile("dmb sy" ::: "memory");
         #endif
      #endif
   #endif
}// nvme_barrier

static inline void nvme_write32(uintptr_t base, uint32_t reg, uint32_t val) {
   if (base == 0 || reg > 0x100000 || (UINTPTR_MAX - base) < (uintptr_t)reg + 4) {
      return;
   }// if
   *(volatile uint32_t *)(base + reg) = val;
}// nvme_write32

static inline uint32_t nvme_read32(uintptr_t base, uint32_t reg) {
   if (base == 0 || reg > 0x100000 || (UINTPTR_MAX - base) < (uintptr_t)reg + 4) {
      return 0xFFFFFFFF;
   }// if
   return *(volatile uint32_t *)(base + reg);
}// nvme_read32

static inline void nvme_write64(uintptr_t base, uint32_t reg, uint64_t val) {
   if (base == 0 || reg > 0x100000 || (UINTPTR_MAX - base) < (uintptr_t)reg + 8) {
      return;
   }// if
   *(volatile uint32_t *)(base + reg) = (uint32_t)(val & 0xFFFFFFFF);
   *(volatile uint32_t *)(base + reg + 4) = (uint32_t)(val >> 32);
}// nvme_write64

static inline uint64_t nvme_read64(uintptr_t base, uint32_t reg) {
   if (base == 0 || reg > 0x100000 || (UINTPTR_MAX - base) < (uintptr_t)reg + 8) {
      return 0xFFFFFFFFFFFFFFFFULL;
   }// if
   uint32_t lo = *(volatile uint32_t *)(base + reg);
   uint32_t hi = *(volatile uint32_t *)(base + reg + 4);
   return ((uint64_t)hi << 32) | lo;
}// nvme_read64

static inline void ring_sq_doorbell(uintptr_t bar_base, uint16_t qid, uint16_t tail) {
   if (qid > 1) {
      return;
   }// if
   uint32_t offset = 0x1000 + (2 * (uint32_t)qid) * db_stride;
   nvme_barrier();
   nvme_write32(bar_base, offset, tail);
}// ring_sq_doorbell

static inline void ring_cq_doorbell(uintptr_t bar_base, uint16_t qid, uint16_t head) {
   if (qid > 1) {
      return;
   }// if
   uint32_t offset = 0x1000 + (2 * (uint32_t)qid + 1) * db_stride;
   nvme_barrier();
   nvme_write32(bar_base, offset, head);
}// ring_cq_doorbell

static int submit_admin_cmd(uintptr_t bar_base, struct nvme_command *cmd, struct nvme_completion *out_cpl) {
   if (bar_base == 0 || !cmd) {
      return -1;
   }// if
   if (nvme_read32(bar_base, NVME_REG_CSTS) & NVME_CSTS_CFS) {
      errorn("nvme: controller fatal status (CFS)");
      nvme_initialized = false;
      return -1;
   }// if

   uint16_t cid = next_cid++;
   if (next_cid == 0) {
      next_cid = 1;
   }// if
   cmd->command_id = cid;

   admin_sq[admin_sq_tail] = *cmd;
   admin_sq_tail = (admin_sq_tail + 1) % ADMIN_QUEUE_SIZE;
   ring_sq_doorbell(bar_base, 0, admin_sq_tail);

   uint32_t timeout = NVME_TIMEOUT_CYCLES;
   volatile struct nvme_completion *cpl = &admin_cq[admin_cq_head];
   while ((cpl->status & 1) != admin_cq_phase) {
      // wait
      if (nvme_read32(bar_base, NVME_REG_CSTS) & NVME_CSTS_CFS) {
         errorn("nvme: controller fatal status (CFS)");
         nvme_initialized = false;
         return -1;
      }// if
      if (--timeout == 0) {
         errorn("nvme: admin command timeout");
         nvme_initialized = false;
         return -1;
      }// if
      asm volatile("nop");
   }// while

   nvme_barrier();
   struct nvme_completion entry = *cpl;

   admin_cq_head = (admin_cq_head + 1) % ADMIN_QUEUE_SIZE;
   if (admin_cq_head == 0) {
      admin_cq_phase ^= 1;
   }// if
   ring_cq_doorbell(bar_base, 0, admin_cq_head);

   if (entry.command_id != cid) {
      errorn("nvme: admin completion command_id mismatch");
      return -1;
   }// if

   if (out_cpl) {
      *out_cpl = entry;
   }// if

   uint16_t status = (entry.status >> 1) & 0x7FFF;
   return (status != 0) ? -1 : 0;
}// submit_admin_cmd

int nvme_init(uintptr_t bar_base) {
   infon("Probing NVM Express (NVMe) PCIe Storage controller");
   if (bar_base == 0 || (bar_base & 0xFFF) != 0 || bar_base > (UINTPTR_MAX - 0x100000)) {
      errorn("nvme: invalid MMIO BAR address");
      return -1;
   }// if

   uint32_t vs = nvme_read32(bar_base, NVME_REG_VS);
   if (vs == 0 || vs == 0xFFFFFFFF) {
      errorn("nvme: controller unreadable");
      return -1;
   }// if

   uint64_t cap = nvme_read64(bar_base, NVME_REG_CAP);
   if (cap == 0 || cap == 0xFFFFFFFFFFFFFFFFULL) {
      errorn("nvme: invalid CAP register");
      return -1;
   }// if

   uint32_t dstrd = (uint32_t)((cap >> 32) & 0xF);
   if (dstrd > 4) {
      errorn("nvme: unsupported doorbell stride");
      return -1;
   }// if
   db_stride = 4 << dstrd;

   uint32_t mqes = (uint32_t)(cap & 0xFFFF) + 1;
   if (mqes < ADMIN_QUEUE_SIZE || mqes < IO_QUEUE_SIZE) {
      errorn("nvme: controller queue capacity too small");
      return -1;
   }// if

   if (nvme_read32(bar_base, NVME_REG_CSTS) & NVME_CSTS_CFS) {
      errorn("nvme: controller fatal status (CFS)");
      return -1;
   }// if

   uint32_t cc = nvme_read32(bar_base, NVME_REG_CC);
   if (cc & NVME_CC_ENABLE) {
      nvme_write32(bar_base, NVME_REG_CC, cc & ~NVME_CC_ENABLE);
   }// if

   uint32_t timeout = NVME_TIMEOUT_CYCLES;
   while (nvme_read32(bar_base, NVME_REG_CSTS) & NVME_CSTS_RDY) {
      // wait
      if (nvme_read32(bar_base, NVME_REG_CSTS) & NVME_CSTS_CFS) {
         errorn("nvme: controller fatal status during reset");
         return -1;
      }// if
      if (--timeout == 0) {
         errorn("nvme: controller reset timeout");
         return -1;
      }// if
   }// while

   memset(admin_sq, 0, sizeof(admin_sq));
   memset(admin_cq, 0, sizeof(admin_cq));
   memset(prp_list, 0, sizeof(prp_list));
   admin_sq_tail = 0;
   admin_cq_head = 0;
   admin_cq_phase = 1;
   next_cid = 1;
   nvme_locked = false;

   uint32_t aqa = ((ADMIN_QUEUE_SIZE - 1) << 16) | (ADMIN_QUEUE_SIZE - 1);
   nvme_write32(bar_base, NVME_REG_AQA, aqa);
   nvme_barrier();
   nvme_write64(bar_base, NVME_REG_ASQ, (uint64_t)(uintptr_t)admin_sq);
   nvme_write64(bar_base, NVME_REG_ACQ, (uint64_t)(uintptr_t)admin_cq);
   nvme_barrier();

   cc = NVME_CC_ENABLE | NVME_CC_CSS_NVM | NVME_CC_MPS_4K | NVME_CC_IOSQES_64 | NVME_CC_IOCQES_16;
   nvme_write32(bar_base, NVME_REG_CC, cc);

   timeout = NVME_TIMEOUT_CYCLES;
   while (!(nvme_read32(bar_base, NVME_REG_CSTS) & NVME_CSTS_RDY)) {
      // wait
      if (nvme_read32(bar_base, NVME_REG_CSTS) & NVME_CSTS_CFS) {
         errorn("nvme: controller fatal status (CFS)");
         return -1;
      }// if
      if (--timeout == 0) {
         errorn("nvme: controller enable timeout");
         return -1;
      }// if
   }// while

   memset(io_cq, 0, sizeof(io_cq));
   io_cq_head = 0;
   io_cq_phase = 1;

   struct nvme_command cmd;
   memset(&cmd, 0, sizeof(cmd));
   cmd.opcode = NVME_ADMIN_OP_CREATE_CQ;
   cmd.prp1 = (uint64_t)(uintptr_t)io_cq;
   cmd.cdw10 = ((IO_QUEUE_SIZE - 1) << 16) | 1;
   cmd.cdw11 = 0x1;
   if (submit_admin_cmd(bar_base, &cmd, NULL) != 0) {
      errorn("nvme: failed to create I/O CQ");
      return -1;
   }// if

   memset(io_sq, 0, sizeof(io_sq));
   io_sq_tail = 0;

   memset(&cmd, 0, sizeof(cmd));
   cmd.opcode = NVME_ADMIN_OP_CREATE_SQ;
   cmd.prp1 = (uint64_t)(uintptr_t)io_sq;
   cmd.cdw10 = ((IO_QUEUE_SIZE - 1) << 16) | 1;
   cmd.cdw11 = (1 << 16) | 0x1;
   if (submit_admin_cmd(bar_base, &cmd, NULL) != 0) {
      errorn("nvme: failed to create I/O SQ");
      return -1;
   }// if

   active_bar_base = bar_base;
   nvme_initialized = true;
   successn("nvme: storage controller initialized successfully");
   return 0;
}// nvme_init

static int nvme_io_transfer_single(uintptr_t bar_base, uint8_t opcode, uint64_t lba, uint32_t count, void *buf) {
   if (!nvme_initialized || bar_base == 0 || bar_base != active_bar_base || !buf || count == 0) {
      return -1;
   }// if
   if (count > NVME_MAX_TRANSFER_BLOCKS) {
      return -1;
   }// if
   if (((uintptr_t)buf & 3) != 0) {
      return -1;
   }// if
   if (lba + (uint64_t)count < lba) {
      return -1;
   }// if

   uint64_t total_bytes = (uint64_t)count * NVME_SECTOR_SIZE;
   if ((uint64_t)(uintptr_t)buf + total_bytes > UINTPTR_MAX) {
      return -1;
   }// if

   if (nvme_read32(bar_base, NVME_REG_CSTS) & NVME_CSTS_CFS) {
      errorn("nvme: controller fatal status (CFS)");
      nvme_initialized = false;
      return -1;
   }// if

   uintptr_t buf_addr = (uintptr_t)buf;
   uint32_t page_offset = (uint32_t)(buf_addr & (NVME_PAGE_SIZE - 1));
   uint32_t first_page_bytes = NVME_PAGE_SIZE - page_offset;

   if (total_bytes > first_page_bytes && (buf_addr & (NVME_SECTOR_SIZE - 1)) != 0) {
      return -1;
   }// if

   uint16_t cid = next_cid++;
   if (next_cid == 0) {
      next_cid = 1;
   }// if

   struct nvme_command *cmd = &io_sq[io_sq_tail];
   memset(cmd, 0, sizeof(*cmd));
   cmd->opcode = opcode;
   cmd->command_id = cid;
   cmd->nsid = 1;
   cmd->prp1 = (uint64_t)buf_addr;
   cmd->cdw10 = (uint32_t)(lba & 0xFFFFFFFF);
   cmd->cdw11 = (uint32_t)(lba >> 32);
   cmd->cdw12 = (count - 1) & 0xFFFF;

   if (total_bytes > first_page_bytes) {
      uintptr_t next_addr = buf_addr + first_page_bytes;
      uint64_t remaining_bytes = total_bytes - first_page_bytes;

      if (remaining_bytes <= NVME_PAGE_SIZE) {
         cmd->prp2 = (uint64_t)next_addr;
      }// if
      else {
         memset(prp_list, 0, sizeof(prp_list));
         cmd->prp2 = (uint64_t)(uintptr_t)prp_list;
         uint32_t prp_entries = 0;
         while (remaining_bytes > 0) {
            if (prp_entries >= NVME_MAX_PRP_LIST_ENTRIES) {
               return -1;
            }// if
            prp_list[prp_entries++] = (uint64_t)next_addr;
            if (remaining_bytes <= NVME_PAGE_SIZE) {
               break;
            }// if
            remaining_bytes -= NVME_PAGE_SIZE;
            next_addr += NVME_PAGE_SIZE;
         }// while
      }// else
   }// if

   nvme_barrier();

   io_sq_tail = (io_sq_tail + 1) % IO_QUEUE_SIZE;
   ring_sq_doorbell(bar_base, 1, io_sq_tail);

   uint32_t timeout = NVME_TIMEOUT_CYCLES;
   volatile struct nvme_completion *cpl = &io_cq[io_cq_head];
   while ((cpl->status & 1) != io_cq_phase) {
      // wait
      if (nvme_read32(bar_base, NVME_REG_CSTS) & NVME_CSTS_CFS) {
         errorn("nvme: controller fatal status (CFS)");
         nvme_initialized = false;
         return -1;
      }// if
      if (--timeout == 0) {
         errorn("nvme: I/O command timeout");
         nvme_initialized = false;
         return -1;
      }// if
      asm volatile("nop");
   }// while

   nvme_barrier();
   struct nvme_completion entry = *cpl;

   io_cq_head = (io_cq_head + 1) % IO_QUEUE_SIZE;
   if (io_cq_head == 0) {
      io_cq_phase ^= 1;
   }// if
   ring_cq_doorbell(bar_base, 1, io_cq_head);

   if (entry.command_id != cid) {
      errorn("nvme: I/O completion command_id mismatch");
      return -1;
   }// if

   uint16_t status = (entry.status >> 1) & 0x7FFF;
   return (status != 0) ? -1 : 0;
}// nvme_io_transfer_single

static int nvme_io_transfer(uintptr_t bar_base, uint8_t opcode, uint64_t lba, uint32_t count, void *buf) {
   if (!nvme_initialized || bar_base == 0 || bar_base != active_bar_base || !buf || count == 0) {
      return -1;
   }// if
   if (lba + (uint64_t)count < lba) {
      return -1;
   }// if
   if (((uintptr_t)buf & 3) != 0) {
      return -1;
   }// if

   uint64_t total_bytes = (uint64_t)count * NVME_SECTOR_SIZE;
   if ((uint64_t)(uintptr_t)buf + total_bytes > UINTPTR_MAX) {
      return -1;
   }// if

   if (nvme_locked) {
      return -1;
   }// if
   nvme_locked = true;

   uint8_t *ptr = (uint8_t *)buf;
   uint64_t cur_lba = lba;
   uint32_t rem = count;
   int ret = 0;

   while (rem > 0) {
      uint32_t chunk = rem;
      if (chunk > NVME_MAX_TRANSFER_BLOCKS) {
         chunk = NVME_MAX_TRANSFER_BLOCKS;
      }// if
      ret = nvme_io_transfer_single(bar_base, opcode, cur_lba, chunk, ptr);
      if (ret != 0) {
         break;
      }// if
      cur_lba += chunk;
      ptr += (size_t)chunk * NVME_SECTOR_SIZE;
      rem -= chunk;
   }// while

   nvme_locked = false;
   return ret;
}// nvme_io_transfer

int nvme_read_blocks(uintptr_t bar_base, uint64_t lba, uint32_t count, void *buf) {
   return nvme_io_transfer(bar_base, NVME_NVM_OP_READ, lba, count, buf);
}// nvme_read_blocks

int nvme_write_blocks(uintptr_t bar_base, uint64_t lba, uint32_t count, const void *buf) {
   return nvme_io_transfer(bar_base, NVME_NVM_OP_WRITE, lba, count, (void *)buf);
}// nvme_write_blocks
