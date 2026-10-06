#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bool.h>
#include "screen/status.c"
#include "screen/screen.c"
#include "utils/concat.c"
#include "clock/clock.c"

extern void kernelInit() {
   start_screen();
   print("Kernel Starting");
} // kernelInit
