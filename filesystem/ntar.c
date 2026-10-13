#include "../include/ntar.h"
#include "../include/kernel.h"
#include "../include/userspace.h"
#include "../include/memory.h"
#include "../include/hardware.h"

#ifndef NTAR_HASH_BUCKETS
#define NTAR_HASH_BUCKETS 64
#endif

#ifndef NTAR_SECTOR_SIZE
#define NTAR_SECTOR_SIZE 512
#endif

static volatile bool ntar_active = false;
static volatile uint8_t ntar_lock = 0;
static struct ntar_sector ntar_cache[NTAR_MAX_RAM_SECTORS];
static uint32_t ntar_used_count = 0;
static int16_t ntar_hash_table[NTAR_HASH_BUCKETS];
static int16_t ntar_next_chain[NTAR_MAX_RAM_SECTORS];

static inline void ntar_acquire_lock(void) {
   while (__atomic_test_and_set(&ntar_lock, __ATOMIC_ACQUIRE)) {
      // wait
   }// while
}// ntar_acquire_lock

static inline void ntar_release_lock(void) {
   __atomic_clear(&ntar_lock, __ATOMIC_RELEASE);
}// ntar_release_lock

static inline bool ntar_is_valid_ram_buffer(const void *buffer) {
   if (!buffer) {
      return false;
   }// if
   uintptr_t addr = (uintptr_t)buffer;
   if (addr < START_MEMORY || addr > (UINTPTR_MAX - NTAR_SECTOR_SIZE) || (addr + NTAR_SECTOR_SIZE) > END_MEMORY) {
      return false;
   }// if
   if (addr >= START_HARDWARE_ADDRESS && addr < END_HARDWARE_ADDRESS) {
      return false;
   }// if
   return true;
}// ntar_is_valid_ram_buffer

static inline uint32_t ntar_hash(uint64_t sector) {
   uint64_t x = sector;
   x ^= x >> 33;
   x *= 0xff51afd7ed558ccdULL;
   x ^= x >> 33;
   x *= 0xc4ceb9fe1a85ec53ULL;
   x ^= x >> 33;
   return (uint32_t)(x % NTAR_HASH_BUCKETS);
}// ntar_hash

void ntar_reset(void) {
   ntar_acquire_lock();
   ntar_used_count = 0;
   for (uint32_t i = 0; i < NTAR_HASH_BUCKETS; i++) {
      ntar_hash_table[i] = -1;
   }// for
   for (uint32_t i = 0; i < NTAR_MAX_RAM_SECTORS; i++) {
      ntar_next_chain[i] = -1;
   }// for
   memset(ntar_cache, 0, sizeof(ntar_cache));
   ntar_release_lock();
}// ntar_reset

void ntar_init(void) {
   ntar_reset();
   ntar_active = true;
   infon("ntar init");
   successn("Live boot active: disk write protection and update locks engaged");
}// ntar_init

bool ntar_is_active(void) {
#ifdef __USE_NTAR__
   return true;
#else
   return ntar_active;
#endif
}// ntar_is_active

uint32_t ntar_get_used_sectors(void) {
   ntar_acquire_lock();
   uint32_t count = ntar_used_count;
   ntar_release_lock();
   return count;
}// ntar_get_used_sectors

uint32_t ntar_get_used_count(void) {
   ntar_acquire_lock();
   uint32_t count = ntar_used_count;
   ntar_release_lock();
   return count;
}// ntar_get_used_count

static int find_sector_index(uint64_t sector) {
   uint32_t bucket = ntar_hash(sector);
   if (bucket >= NTAR_HASH_BUCKETS) {
      return -1;
   }// if
   int16_t idx = ntar_hash_table[bucket];
   uint32_t steps = 0;
   while (idx >= 0 && idx < NTAR_MAX_RAM_SECTORS && steps < NTAR_MAX_RAM_SECTORS) {
      if (!ntar_cache[idx].in_use) {
         break;
      }// if
      if (ntar_cache[idx].sector_num == sector) {
         return idx;
      }// if
      int16_t next = ntar_next_chain[idx];
      if (next == idx || next < 0 || next >= NTAR_MAX_RAM_SECTORS) {
         break;
      }// if
      idx = next;
      steps++;
   }// while
   return -1;
}// find_sector_index

int ntar_ram_write_sector(uint64_t sector, const void *buffer) {
   if (!ntar_is_active()) {
      return -1;
   }// if

   if (!ntar_is_valid_ram_buffer(buffer)) {
      return -1;
   }// if

   if (sector > (UINT64_MAX / NTAR_SECTOR_SIZE)) {
      return -1;
   }// if

   ntar_acquire_lock();

   int existing_idx = find_sector_index(sector);
   if (existing_idx >= 0 && existing_idx < NTAR_MAX_RAM_SECTORS) {
      memcpy(ntar_cache[existing_idx].data, buffer, NTAR_SECTOR_SIZE);
      ntar_release_lock();
      return 0;
   }// if

   if (ntar_used_count >= NTAR_MAX_RAM_SECTORS) {
      ntar_release_lock();
      errorn("ntar ram overlay capacity reached; disk write blocked to preserve host state");
      return -1;
   }// if

   int free_idx = -1;
   for (uint32_t i = 0; i < NTAR_MAX_RAM_SECTORS; i++) {
      if (!ntar_cache[i].in_use) {
         free_idx = (int)i;
         break;
      }// if
   }// for

   if (free_idx < 0 || free_idx >= NTAR_MAX_RAM_SECTORS) {
      ntar_release_lock();
      return -1;
   }// if

   uint32_t bucket = ntar_hash(sector);
   if (bucket >= NTAR_HASH_BUCKETS) {
      ntar_release_lock();
      return -1;
   }// if

   ntar_cache[free_idx].in_use = true;
   ntar_cache[free_idx].sector_num = sector;
   memcpy(ntar_cache[free_idx].data, buffer, NTAR_SECTOR_SIZE);

   int16_t head = ntar_hash_table[bucket];
   if (head < 0 || head >= NTAR_MAX_RAM_SECTORS) {
      head = -1;
   }// if
   ntar_next_chain[free_idx] = head;
   ntar_hash_table[bucket] = (int16_t)free_idx;

   if (ntar_used_count < NTAR_MAX_RAM_SECTORS) {
      ntar_used_count++;
   }// if

   ntar_release_lock();
   return 0;
}// ntar_ram_write_sector

int ntar_ram_read_sector(uint64_t sector, void *buffer) {
   if (!ntar_is_active()) {
      return -1;
   }// if

   if (!ntar_is_valid_ram_buffer(buffer)) {
      return -1;
   }// if

   if (sector > (UINT64_MAX / NTAR_SECTOR_SIZE)) {
      return -1;
   }// if

   ntar_acquire_lock();

   int idx = find_sector_index(sector);
   if (idx >= 0 && idx < NTAR_MAX_RAM_SECTORS) {
      memcpy(buffer, ntar_cache[idx].data, NTAR_SECTOR_SIZE);
      ntar_release_lock();
      return 0;
   }// if

   ntar_release_lock();
   return -1;
}// ntar_ram_read_sector
