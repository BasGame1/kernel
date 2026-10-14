/* This code was obtained from gcc mirror on github
 * https://github.com/gcc-mirror/gcc/blob/master/libiberty/memcmp.c
 * And modified to match the style
 */

#include <ansidecl.h>
#include <stddef.h>

int memcmp (const void *str1, const void *str2, size_t count) {
   register const unsigned char *s1 = (const unsigned char*)str1;
   register const unsigned char *s2 = (const unsigned char*)str2;
   while (count-- > 0) {
       if (*s1++ != *s2++)
	   return s1[-1] < s2[-1] ? -1 : 1;
   }// while
   return 0;
}// memcmp
