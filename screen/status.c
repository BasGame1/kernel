#include "../include/kernel.h"

// Status functions

void debug(char *reason) {
   char *temporal = concat(reason, "\n");
   char *message = concat("\e[0;93m[DEBUG]\e[0;0m ", temporal);
   print(message);
}// debug

void error(char *reason) {
   char *temporal = concat(reason, "\n");
   char *message = concat("\e[0;31m[ERROR]\e[0;0m ", temporal);
   print(message);
}// error

void fatal(char *reason) {
   char *temporal = concat(reason, "\n");
   char *message = concat("\e[0;91m[ERROR]\e[0;0m ", temporal);
   print(message);
   // TODO, add exit function 
}// fatal

void success(char *reason) {
   char *temporal = concat(reason, "\n");
   char *message = concat("\e[0;32m[SUCCESS]\e[0;0m ", temporal);
   print(message);
}// success

void info(char *reason) {
   char *temporal = concat(reason, "\n");
   char *message = concat("\e[0;33m[INFO]\e[0;0m ", temporal);
   print(message);
}// info


// Print wrapper with newline char

void printn(char *message) {
   char *temporal = concat(message, "\n");
   print_character(temporal);
}// printn
