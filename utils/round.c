#include <stdint.h>

uint32_t round_page(uint32_t number, uint32_t page) {
   if (page == 0) {
      return number;
   }// if
   return ((number + page - 1) & ~(page - 1));
}// round_page
