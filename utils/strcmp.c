#include <stddef.h>

int strcmp(const char *s1, const char *s2) {
   while (*s1 && (*s1 == *s2)) {
      s1++;
      s2++;
   }// while
   return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}// strcmp

size_t strlen(const char *s) {
   size_t len = 0;
   while (s && s[len]) {
      len++;
   }// while
   return len;
}// strlen
