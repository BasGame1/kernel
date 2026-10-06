#include <stdio.h>
#include <string.h>
#include "screen.c"
#include "../utils/concat.c"

// Status functions

void debug(char *reason) {
   char *message = concat("\e[0;93m[DEBUG]\e[0;0m", reason);
   print(message);
} // debug

void error(char *reason) {
   char *message = concat("\e[0;31m[ERROR]\e[0;0m", reason);
   print(message);
} // error

void fatal(char *reason) {
   char *message = concat("\e[0;91m[ERROR]\e[0;0m", reason);
   print(message);
 // TODO, add exit function 
} // fatal

void success(char *reason) {
   char *message = concat("\e[0;32m[SUCCESS]\e[0;0m", reason);
   print(message);
} // success

void info(char *reason) {
   char *message = concat("\e[0;3m[INFO]\e[0;0m", reason);
   print(message);
} // info
