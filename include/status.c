#include <stdio.h>
#include <string.h>

// Status functions

void debug(char *reason) {
 printf("\e[0;93m[DEBUG]\e[0;0m %s", reason);

} // debug

void error(char *reason) {
 printf("\e[0;31m[ERROR]\e[0;0m %s", reason);

} // error

void fatal(char *reason) {
 printf("\e[0;91m[FATAL ERROR]\e[0;0m %s", reason);
 // TODO, add exit function 
} // fatal

void success(char *reason) {
 printf("\e[0;32m[SUCCESS]\e[0;0m %s", reason);

} // success

void info(char *reason) {
 printf("\e[0;33m[INFO]\e[0;0m %s", reason);

} // info
