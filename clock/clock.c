#include <stdint.h>

#include "../include/clock.h"
#include "../include/kernel.h"

uint64_t get_mtime() {
#if defined(__RISCV__) || defined(__riscv)
   #if defined(__64BIT__) || defined(__riscv64)
      return *(volatile uint64_t *)CLINT_MTIME_LOW;
   #else
      uint32_t mtime_high_value;
      uint32_t mtime_low_value;
      do {
         mtime_high_value = *CLINT_MTIME_HIGH;
         mtime_low_value = *CLINT_MTIME_LOW;
      } while (mtime_high_value != *CLINT_MTIME_HIGH);
      return (((uint64_t) mtime_high_value) << 32) | mtime_low_value;
   #endif
#elif defined(__X86__) || defined(__i386__) || defined(__x86_64__)
   uint32_t lo, hi;
   asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
   return (((uint64_t)hi) << 32) | lo;
#else
   return 0;
#endif
}// get_mtime

void set_mtimecmp() {
#if defined(__RISCV__) || defined(__riscv)
   uint64_t current_mtime = get_mtime();
   uint64_t next_mtimecmp = current_mtime + INTERVAL;
   #if defined(__64BIT__) || defined(__riscv64)
      *(volatile uint64_t *)CLINT_MTIMECMP_LOW = next_mtimecmp;
   #else
      uint32_t next_high_value = (uint32_t)(next_mtimecmp >> 32);
      uint32_t next_low_value = (uint32_t)(next_mtimecmp & 0xFFFFFFFF);
      *CLINT_MTIMECMP_LOW = 0xFFFFFFFF;
      *CLINT_MTIMECMP_HIGH = next_high_value;
      *CLINT_MTIMECMP_LOW = next_low_value;
   #endif
#endif
}// set_mtimecmp

void init_mtimecmp() {
#if defined(__RISCV__) || defined(__riscv)
   set_mtimecmp();
   set_csrs(mie, 0x80);
   set_csrs(mstatus, 0x8);
#endif
}// init_mtimecmp
