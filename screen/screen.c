#include <stdint.h>

#include "../include/screen.h"

void print_character(char *message) {
   volatile uint8_t *uart_status_pointer = (volatile uint8_t *) UART_STATUS;
   volatile uint8_t *screen_address_pointer = (volatile uint8_t *) SCREEN_ADDRESS;
   uint32_t i = 0;
   while (message[i] != '\0') { 
      while ((*uart_status_pointer & 0x20) == 0) {
         // do not do anything, just wait
      }// while check if uart is free   
      *screen_address_pointer = message[i];
      i++;
   }// while to loop for every character
}// print_character

void clear_screen() {
   print_character("\033[2J\033[H");
}// clear_screen

void start_screen() {
   clear_screen();
   print_character("Hello World!");  
}// start_screen
