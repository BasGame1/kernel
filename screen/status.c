#include "../include/userspace.h"

// Status functions

void debugn(char *reason) {
   char *message = concat("\e[0;93m[DEBUG]\e[0;0m ", reason);
   print(message);
   print("\n");
}// debug

void errorn(char *reason) {
   char *message = concat("\e[0;31m[ERROR]\e[0;0m ", reason);
   print(message);
   print("\n");
}// error

void fataln(char *reason) {
   char *message = concat("\e[0;91m[ERROR]\e[0;0m ", reason);
   print(message);
   print("\n");
   // TODO, add exit function 
}// fatal

void successn(char *reason) {
   char *message = concat("\e[0;32m[SUCCESS]\e[0;0m ", reason);
   print(message);
   print("\n");
}// success

void infon(char *reason) {
   char *message = concat("\e[0;33m[INFO]\e[0;0m ", reason);
   print(message);
   print("\n");
}// info


// Status functions without newline

void debug(char *reason) {
   char *message = concat("\e[0;93m[DEBUG]\e[0;0m ", reason);
   print(message);
}// debug

void error(char *reason) {
   char *message = concat("\e[0;31m[ERROR]\e[0;0m ", reason);
   print(message);
}// error

void fatal(char *reason) {
   char *message = concat("\e[0;91m[ERROR]\e[0;0m ", reason);
   print(message);
   // TODO, add exit function 
}// fatal

void success(char *reason) {
   char *message = concat("\e[0;32m[SUCCESS]\e[0;0m ", reason);
   print(message);
}// success

void info(char *reason) {
   char *message = concat("\e[0;33m[INFO]\e[0;0m ", reason);
   print(message);
}// info


// Print wrapper with newline char

void printn(char *message) {
   char *temporal = concat(message, "\n");
   print_character(temporal);
}// printn
