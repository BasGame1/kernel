#include <stdint.h>
#include <stddef.h>

#include "../../../include/shell.h"
#include "../../../include/kernel.h"

int call_round(int argc, char **argv) {
   if (argc != 3) {
      printn("Usage: round <number> <page>");
      return 1;
   }
   uint32_t number = (uint32_t) parse_number(argv[1]);
   uint32_t page = (uint32_t) parse_number(argv[2]);
   uint32_t final_number = round_page(number, page);
   print("Rounded: ");
   print_hex(final_number);
   printn("");
   return 0;
}// call_round
