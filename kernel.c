#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "include/kernel.h"

#include "screen/status.c"
#include "screen/screen.c"
#include "utils/concat.c"
#include "clock/clock.c"

extern void kernel_init() {
   start_screen();
   print("Kernel Starting");
  //TODO, add calling processes and tbh, everything else
}// kernel_init

extern void trap_handler() {
   //TODO, add trap handler code
}// trap_handler
