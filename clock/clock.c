#include <stdlib.h>

#include "../include/clock.h"
#include "../include/kernel.h"

uint64_t get_mtime() {
   uint32_t mtime_high_value;
   uint32_t mtime_low_value;
   do {
      mtime_high_value = *CLINT_MTIME_HIGH;
      mtime_low_value = *CLINT_MTIME_LOW;
   } while (mtime_high_value != *CLINT_MTIME_HIGH);
   return (((uint64_t)mtime_high_value) << 32) | mtime_low_value;
}// get_mtime

void set_mtimecmp() {
   uint64_t current_mtime = get_mtime();
   uint64_t next_mtimecmp = current_mtime + INTERVAL;
   uint32_t next_high_value = (uint32_t)(next_mtimecmp >> 32);
   uint32_t next_low_value = (uint32_t)(next_mtimecmp & 0xFFFFFFFF);
   *CLINT_MTIMECMP_LOW = 0xFFFFFFFF;
   *CLINT_MTIMECMP_HIGH = next_high_value;
   *CLINT_MTIMECMP_LOW = next_low_value;
}// set_mtimecp

void init_mtimecmp() {
   set_mtimemcp();
   set_csrs(mie, 0x80);
   set_csrs(mstatus, 0x8);
}// init_mtimemcp
