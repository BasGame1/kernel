#include <stdint.h>
#include <stdbool.h>

#include "../../../include/kernel.h"
#include "../../../include/shell.h"

#define COLOR_RESET "\033[0m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"

static void neo_usage(void) {
   printn(YELLOW "|--------------------------USAGE------------------------|" COLOR_RESET);
   printn(BLUE   "|                                                       |" COLOR_RESET);
   printn(YELLOW "|add as arguments the path of the files you want to open|" COLOR_RESET);
   printn(BLUE   "|             example: neo /proc/meminfo                |" COLOR_RESET);
   printn(YELLOW "| for opening this menu call it with -h, help or --help |" COLOR_RESET);
   printn(BLUE   "|                                                       |" COLOR_RESET);
   printn(YELLOW "|_______________________________________________________|" COLOR_RESET);
}// neo_usage

int call_neo(int argc, char **argv) {
   if (argc < 2) {
      info("No arguments given");
      printn("");
      neo_usage();
      return 1;
   }
   if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0) {
      neo_usage();
      return 0;
   }
   for (int i = 1; i < argc; i++) {
      info("Opening file: ");
      printn(argv[i]);
      errorn("neo: filesystem not implemented");
   }
   return 0;
}// call_neo
