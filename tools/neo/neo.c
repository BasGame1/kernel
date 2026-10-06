#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "neo.h"
#include "../../include/screen.h"

int main(int argc, char *argv[]) {
 if (argv[1] == NULL) {
  info("No arguments");
  return 1;
 }
 for (int i = 1; i < argc; i++) {
  char file_to_open[256];
  strncpy(file_to_open, argv[i], sizeof(file_to_open) - 1);
  file_to_open[sizeof(file_to_open) - 1] = '\0';
  if (strcmp("help",file_to_open) == 0 || strcmp("-h",file_to_open) == 0 || strcmp("--help",file_to_open) == 0 ) {
   info("help options");
   return 1;
  }
  FILE *file_opened = fopen(file_to_open, "r");
   if (file_opened == NULL ) {
    info("Could not open file");
    return 1;
   }
  if (!argv[2] == NULL) {
   info_text("--OPENING FILE NUMBER", false);
   printf("%d--\n", i);
   read_file(file_opened);
   info_text("-------------------------", true);
  } else {
   read_file(file_opened);
  }
 }
 return 0;
}
void read_file(FILE *choosen_file) {
 char line_buffer[256];
 while (fgets(line_buffer, sizeof(line_buffer), choosen_file)) {
  printf("%s", line_buffer);
 }
 fclose(choosen_file);
}
void error_text(char text[],bool if_new_line) {
 if (if_new_line) {
  printf(RED "ERROR: %s\n" COLOR_RESET, text);
 } else {
  printf(RED "ERROR: %s" COLOR_RESET, text);
 } 
}
void info_text(char text[],bool if_new_line) {
 if (if_new_line) {
  printf(YELLOW "INFO: %s\n" COLOR_RESET, text);
 } else {
  printf(YELLOW "INFO: %s" COLOR_RESET, text);
 }
}
void success_text(char text[],bool if_new_line) {
 if (if_new_line) {
  printf(GREEN "SUCESS: %s\n" COLOR_RESET, text);
 } else {
  printf(GREEN "SUCESS: %s" COLOR_RESET, text);
 }
}
void info(char call_reason[]) {
 if (!strcmp("help options",call_reason) == 0 ) {
  error_text(call_reason, true);
 }
 printf(YELLOW"|--------------------------USAGE------------------------|\n" COLOR_RESET);
 printf(BLUE "|                                                       |\n" COLOR_RESET);
 printf(YELLOW "|add as arguments the path of the files you want to open|\n" COLOR_RESET);
 printf(BLUE "|             example: ./$0 /proc/meminfo               |\n" COLOR_RESET);
 printf(YELLOW "| for opening this menu call it with -h, help or --help |\n" COLOR_RESET);
 printf(BLUE "|                                                       |\n" COLOR_RESET);
 printf(YELLOW "|_______________________________________________________|\n" COLOR_RESET);
}
