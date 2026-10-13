#include <stddef.h>

char* concat(char *hardcoded_text, char *reason) {
   static char combination[4][256];
   static size_t buf_idx = 0;

   char *dest = combination[buf_idx];
   buf_idx = (buf_idx + 1) % 4;

   size_t i = 0;
   if (hardcoded_text) {
      while (*hardcoded_text && i < 255) {
         dest[i++] = *hardcoded_text++;
      }// while
   }// if

   if (reason) {
      while (*reason && i < 255) {
         dest[i++] = *reason++;
      }// while
   }// if

   dest[i] = '\0';
   return dest;
}// concat
