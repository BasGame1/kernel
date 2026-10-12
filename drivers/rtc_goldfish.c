// drivers/rtc_goldfish.c
/*
 * Ported from the Linux kernel (drivers/rtc/rtc-goldfish.c)
 * Copyright (C) Linus Torvalds and Linux kernel contributors.
 * Licensed under GNU General Public License (GPL) v2.0.
 * Adapted for Antartica microkernel hardware abstraction layer.
 */

#include <stddef.h>
#include "../include/drivers.h"

#define TIMER_TIME_LOW  0x00
#define TIMER_TIME_HIGH 0x04
#define TIMER_ALARM_LOW 0x08
#define TIMER_ALARM_HIGH 0x0c
#define TIMER_IRQ_ENABLED 0x10
#define TIMER_CLEAR_ALARM 0x14
#define TIMER_ALARM_STATUS 0x18
#define TIMER_CLEAR_INTERRUPT 0x1c

static inline uint32_t rtc_read(uintptr_t base, uint32_t offset) {
   return *(volatile uint32_t *)(base + offset);
}// rtc_read

uint64_t rtc_goldfish_read_ns(uintptr_t base_addr) {
   uint32_t low = rtc_read(base_addr, TIMER_TIME_LOW);
   uint32_t high = rtc_read(base_addr, TIMER_TIME_HIGH);
   return ((uint64_t)high << 32) | (uint64_t)low;
}// rtc_goldfish_read_ns

static const int days_per_month[12] = {
   31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static bool is_leap_year(int year) {
   return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}// is_leap_year

static uint64_t divmod64(uint64_t n, uint32_t d, uint32_t *rem) {
   uint64_t q = 0;
   uint64_t r = 0;
   for (int i = 63; i >= 0; i--) {
      r = (r << 1) | ((n >> i) & 1);
      if (r >= d) {
         r -= d;
         q |= (1ULL << i);
      }// if
   }// for
   if (rem) {
      *rem = (uint32_t)r;
   }// if
   return q;
}// divmod64

int rtc_goldfish_get_time(uintptr_t base_addr, struct rtc_time *tm) {
   if (!tm) {
      return -1;
   }// if
   uint64_t ns = rtc_goldfish_read_ns(base_addr);
   uint64_t sec = divmod64(ns, 1000000000U, NULL);

   uint32_t s = 0, m = 0, h = 0;
   uint64_t total_min = divmod64(sec, 60, &s);
   tm->sec = (int)s;

   uint64_t total_hours = divmod64(total_min, 60, &m);
   tm->min = (int)m;

   uint64_t days = divmod64(total_hours, 24, &h);
   tm->hour = (int)h;

   // Epoch 1970
   int year = 1970;
   while (1) {
      int days_in_year = is_leap_year(year) ? 366 : 365;
      if (days < (uint64_t)days_in_year) {
         break;
      }// if
      days -= days_in_year;
      year++;
   }// while
   tm->year = year;

   int month = 0;
   while (month < 12) {
      int d = days_per_month[month];
      if (month == 1 && is_leap_year(year)) {
         d = 29;
      }// if
      if (days < (uint64_t)d) {
         break;
      }// if
      days -= d;
      month++;
   }// while
   tm->mon = month + 1;
   tm->mday = (int)days + 1;
   return 0;
}// rtc_goldfish_get_time
