/* From https://github.com/gcc-mirror/gcc/blob/master/libgcc/memset.c  
 * Has been modified to match the style of the rest of the code
 */

#include <stddef.h>

void *memset (void *dest, int val, size_t len) {
   unsigned char *ptr = dest;
   while (len-- > 0)
      *ptr++ = val;
   return dest;
}// memset
