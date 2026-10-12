#include <stddef.h>
#include <stdint.h>

#include "../../../include/shell.h"
#include "../../../include/kernel.h"

int call_memset(int argc, char **argv) {
   if (argc != 4) {
      printn("Usage: memset <address> <char/val> <size>");
      return 1;
   }
   extern int _end;
   uintptr_t dest_addr = (uintptr_t) parse_number(argv[1]);
   int val = (int) parse_number(argv[2]);
   size_t len = (size_t) parse_number(argv[3]);
   if (dest_addr < (uintptr_t)&_end || dest_addr + len < dest_addr) {
      errorn("memset: memory address out of bounds (kernel protected)");
      return 1;
   }
   void *dest = (void *) dest_addr;
   unsigned char *ptr = (unsigned char *) dest;
   size_t remaining = len;
   while (remaining-- > 0) {
      *ptr++ = (unsigned char) val;
   }
   successn("Memory filled successfully");
   return 0;
}// call_memset
