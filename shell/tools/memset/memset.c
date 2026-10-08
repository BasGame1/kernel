/* From https://github.com/gcc-mirror/gcc/blob/master/libgcc/memset.c  
 * It has been modified for this project
 */

#include <stddef.h>

#include "../../screen/screen.c"
#include "../../utils/parse_number.c"

void *main (int argc, char **argv) {
   if (argc != 3) {
      print("Usage: memset( *<start address>, int <value to replace with>, size_t <lenght>"); 
   } else {
      void *dest = (void*) parse_number(argv[1]);
      int val = (int) parse_number(argv[2]);
      size_t len = (size_t) argv[3];
      unsigned char *ptr = dest;
      while (len-- > 0)
         *ptr++ = val;
      return dest;
   }
}// memset
