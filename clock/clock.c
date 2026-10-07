#include <stdint.h>

#include "../include/clock.h"
#include "../include/kernel.h"

uintptr_t get_mtime() {
   uintptr_t mtime_high_value;
   uintptr_t mtime_low_value;
   do {
      mtime_high_value = *CLINT_MTIME_HIGH;
      mtime_low_value = *CLINT_MTIME_LOW;
   } while (mtime_high_value != *CLINT_MTIME_HIGH);
   return (mtime_high_value << 32) | mtime_low_value;
}// get_mtime

void set_mtimecmp() {
   uintptr_t current_mtime = get_mtime();
   uintptr_t next_mtimecmp = current_mtime + INTERVAL;
   uintptr_t next_high_value = (uintptr_t)(next_mtimecmp >> 32);
   uintptr_t next_low_value = (uintptr_t)(next_mtimecmp & 0xFFFFFFFF);
   *CLINT_MTIMECMP_LOW = 0xFFFFFFFF;
   *CLINT_MTIMECMP_HIGH = next_high_value;
   *CLINT_MTIMECMP_LOW = next_low_value;
}// set_mtimecp

void init_mtimecmp() {
   set_mtimecmp();
   set_csrs(mie, 0x80);
   set_csrs(mstatus, 0x8);
}// init_mtimemcp
