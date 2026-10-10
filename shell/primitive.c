#include <stdint.h>
#include <stdbool.h>

#include "../include/info.h"
#include "../include/kernel.h"
#include "../include/keyboard.h"
#include "../include/shell.h"

static int split_command(char *line, char **argv, int max_args) {
   int argc = 0;
   while (*line && argc < max_args) {
      while (*line == ' ' || *line == '\t' || *line == '\n' || *line == '\r') {
         *line = '\0';
         line++;
      }
      if (*line == '\0') {
         break;
      }
      argv[argc++] = line;
      while (*line && *line != ' ' && *line != '\t' && *line != '\n' && *line != '\r') {
         line++;
      }
   }
   return argc;
}// split_command

int check_command(char *full_command) {
   char *argv[16];
   int argc = split_command(full_command, argv, 16);
   if (argc == 0) {
      return 0;
   }
   char *command = argv[0];
   // Bad code, ik, but atm idk any other easy and quick alternative to this monster
   if (strcmp(command, "neo") == 0) {
      call_neo(argc, argv);
   } else if (strcmp(command, "help") == 0) {
      call_help();
   } else if (strcmp(command, "memset") == 0) {
      call_memset(argc, argv);
   } else if (strcmp(command, "round") == 0) {
      call_round(argc, argv);
   } else if (strcmp(command, "info") == 0) {
      call_info();
   } else if (strcmp(command, "version") == 0) {
      call_version();
   } else if (strcmp(command, "clear") == 0) {
      clear_screen();
   } else if (strcmp(command, "clean") == 0) {
      clear_screen();
   } else if (strcmp(command, "logo") == 0) {
      print_asccii_art();
   } else if (strcmp(command, "ls") == 0) {
      call_ls(argc, argv);
   } else if (strcmp(command, "memcpy") == 0) {
      call_memcpy(argc, argv);
   } else if (strcmp(command, "exit") == 0) {
      return 1;
   } else {
      call_error(command);
   }
   return 0;
}// check_command

void wait_command(void) {
   while (true) {
      print("primitive> ");
      char *full_command = get_input();
      if (check_command(full_command)) {
         break;
      }
   }
}// wait_command

void init_shell(void) {
   infon("Starting shell");
   wait_command();
   infon("Exited shell");
}// init_shell

void call_help(void) {
   printn("---HELP MENU---");
   printn("Usage: <command> <argument1> <argument2>");
   printn("Command list: ");
   printn("neo <file1> <file2> - To get the contents of files");
   printn("memset <address> <char to fill> <size of the block> - To fill big blocks of data with a char");
   printn("round <number to round> <number to round to> - To round a number to another");
   printn("help - To show this menu");
   printn("version - To show info about the kernel");
   printn("info - Same as version");
   printn("clean - Clears the screen");
   printn("clear - Same as clean");
   printn("ls - List files");
   printn("memcpy <destiny address> <origin address> <size> - Copys a block of data from one address to another ");
   printn("logo - Prints the polar bear logo");
   printn("exit - Exit the shell");
}// call_help

void call_error(char *command) {
   print("Command: ");
   print(command);
   printn(" does not exist");
}// call_error

void call_info(void) {
   print("Kernel version is: ");
   printn(KERNEL_VERSION);
   print("Shell version is: ");
   printn(SHELL_VERSION);
   print("Other info: ");
   printn(OTHER_INFO);
}// call_info

void call_version(void) {
   call_info();
}// call_version

void call_ls(argc, argv) {
   if (argc != 1) {
      ls(argv);
   } else {
      errorn("ls should only have 1 arg");
   }// if
}// call_ls
void call_memcpy(argc, argv) {
   if (argc != 3) {
      memcpy(argv[1], argv[2], argv[3]);
   } else {
      errorn("ls should only have 3 args");
   }// if
}// call_memcpy
