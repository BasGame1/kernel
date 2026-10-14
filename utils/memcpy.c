#include <stddef.h>
#include <stdint.h>

void *memcpy(void *dest, const void *src, size_t n) {
   char *d = (char *)dest;
   const char *s = (const char *)src;

   if (n >= sizeof(uintptr_t) && ((((uintptr_t)d | (uintptr_t)s) & (sizeof(uintptr_t) - 1)) == 0)) {
      uintptr_t *dw = (uintptr_t *)d;
      const uintptr_t *sw = (const uintptr_t *)s;
      size_t words = n / sizeof(uintptr_t);
      while (words--) {
         *dw++ = *sw++;
      }// while
      d = (char *)dw;
      s = (const char *)sw;
      n %= sizeof(uintptr_t);
   }// if

   while (n--) {
      *d++ = *s++;
   }// while
   return dest;
}// memcpy
