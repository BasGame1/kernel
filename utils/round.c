#include <stdint.h>

uint32_t round_page(uint32_t number, uint32_t page) {
   return ((number + page) & 0xFFFFF000);
}// round
