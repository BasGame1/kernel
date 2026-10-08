#include <stddef.h>
#include <stdint.h>

#include "../../../include/shell.h"
#include "../../../include/kernel.h"

int call_memset(int argc, char **argv) {
   if (argc != 4) {
      printn("Usage: memset <address> <char/val> <size>");
      return 1;
   }
   void *dest = (void *) parse_number(argv[1]);
   int val = (int) parse_number(argv[2]);
   size_t len = (size_t) parse_number(argv[3]);
   unsigned char *ptr = (unsigned char *) dest;
   size_t remaining = len;
   while (remaining-- > 0) {
      *ptr++ = (unsigned char) val;
   }
   successn("Memory filled successfully");
   return 0;
}// call_memset
