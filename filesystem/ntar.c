#include "../include/ntar.h"
#include "../include/kernel.h"
#include "../include/userspace.h"

static bool ntar_active = false;
static struct ntar_sector ntar_cache[NTAR_MAX_RAM_SECTORS];

void ntar_init(void) {
   ntar_active = true;
   for (uint32_t i = 0; i < NTAR_MAX_RAM_SECTORS; i++) {
      ntar_cache[i].in_use = false;
      ntar_cache[i].sector_num = 0;
   }// for
   infon("natr init");
   successn("Live boot active: disk write protection and update locks engaged");
}// ntar_init

bool ntar_is_active(void) {
#ifdef __USE_NTAR__
   return true;
#else
   return ntar_active;
#endif
}// ntar_is_active

int ntar_ram_write_sector(uint64_t sector, const void *buffer) {
   if (!buffer) {
      return -1;
   }// if
   for (uint32_t i = 0; i < NTAR_MAX_RAM_SECTORS; i++) {
      if (ntar_cache[i].in_use && ntar_cache[i].sector_num == sector) {
         memcpy(ntar_cache[i].data, buffer, 512);
         return 0;
      }// if
   }// for
   for (uint32_t i = 0; i < NTAR_MAX_RAM_SECTORS; i++) {
      if (!ntar_cache[i].in_use) {
         ntar_cache[i].in_use = true;
         ntar_cache[i].sector_num = sector;
         memcpy(ntar_cache[i].data, buffer, 512);
         return 0;
      }// if
   }// for
   errorn("ntar ram overlay capacity reached; disk write blocked to preserve host state");
   return 0;
}// ntar_ram_write_sector

int ntar_ram_read_sector(uint64_t sector, void *buffer) {
   if (!buffer) {
      return -1;
   }// if
   for (uint32_t i = 0; i < NTAR_MAX_RAM_SECTORS; i++) {
      if (ntar_cache[i].in_use && ntar_cache[i].sector_num == sector) {
         memcpy(buffer, ntar_cache[i].data, 512);
         return 0;
      }// if
   }// for
   return -1;
}// ntar_ram_read_sector
