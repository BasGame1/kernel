#include <stdint.h>
#include "../include/kernel.h"

void print_hex(uint32_t val) {
   char buf[11];
   buf[0] = '0';
   buf[1] = 'x';
   for (int i = 7; i >= 0; i--) {
      uint8_t nibble = (val >> (i * 4)) & 0x0F;
      buf[2 + (7 - i)] = (nibble < 10) ? ('0' + nibble) : ('a' + (nibble - 10));
   }
   buf[10] = '\0';
   print(buf);
}// print_hex

void print_dec(uint32_t val) {
   if (val == 0) {
      print("0");
      return;
   }
   char buf[12];
   int i = 10;
   buf[11] = '\0';
   while (val > 0 && i >= 0) {
      buf[i--] = '0' + (val % 10);
      val /= 10;
   }
   print(&buf[i + 1]);
}// print_dec
