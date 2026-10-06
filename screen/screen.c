#include <stdio.h>
#include <stdlib.h>
#include "../include/screen.h"

void print_character(char *message) {
   char *screen = (char *) SCREEN_ADR;
   int message_size = sizeof(message);
   for (int i = 0; i < message_size; i = i + 2) {
      if (message[i] == '\n') {
         position = MAX_COL;
      } else {
         position = i;
      }// if for newline char
      
      screen[position] = message[i];

      // TODO, change hardcoded style to a dynamically injected one
      screen[position+1] = 0x1F;
   }// for characters array 
}// print_character

void clear_screen() {
   for (int i = 0; i < MAX_COL; i++) { 
       for (int n = 0; n < MAX_ROW; n++) { 
          print_character('\0');
       }// for rows
   }// for columns 
}//clear_sceen

// Start screen clears and prepares the screen

void start_screen() {
   // empty the screen
   clear_screen();

   // print characters
   print_character("Hello World!");  
}// start_screen

