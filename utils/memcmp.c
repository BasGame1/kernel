#include <stddef.h>

int memcmp (const void *str1, const void *str2, size_t count) {
   const unsigned char *s1 = (const unsigned char*)str1;
   const unsigned char *s2 = (const unsigned char*)str2;
   while (count-- > 0) {
      if (*s1++ != *s2++) {
         return s1[-1] < s2[-1] ? -1 : 1;
      }// if
   }// while
   return 0;
}// memcmp
