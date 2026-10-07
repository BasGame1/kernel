#include <stddef.h>

size_t parse_number(char *str) {
   size_t result = 0;
   if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
      str += 2;
      while (*str) {
         result *= 16;
         if (*str >= '0' && *str <= '9') result += *str - '0';
         else if (*str >= 'a' && *str <= 'f') result += *str - 'a' + 10;
         else if (*str >= 'A' && *str <= 'F') result += *str - 'A' + 10;
         str++;
      }
   } else {
      while (*str >= '0' && *str <= '9') {
         result = result * 10 + (*str - '0');
         str++;
      }
   }
   return result;
}// parse_number
