#include <stdint.h>

#include "../include/hardware.h"
#include "../include/keyboard.h"

char get_char() {
   volatile char input;
   volatile uint8_t *uart_status_pointer = (volatile uint8_t *) UART_STATUS;
   volatile uint8_t *screen_address_pointer = (volatile uint8_t *) SCREEN_ADDRESS;
   while ((*uart_status_pointer & 0x01) == 0) {
      // wait
   }// while to wait
   input = (char) *screen_address_pointer;
   return input;
}// get_char
    
void print_input() {
   uint32_t i = 0;
   while (i < TEXT_BUFFER) {
      char character = get_char();
      if ((character == '\r') || (character == '\n')) {
         print("\n");
         break;
      } else if ((character == '\b') || (character == 127)) {
         if (i > 0) {
            print("\b \b");
            i--;
         }// if backspace
      } else {
         char echo[2] = {character, '\0'};
         print(echo);
         i++;
      }//if special chars
   }// while
}// print_input

char *get_input() {
   static char character_list[TEXT_BUFFER];
   uint32_t i = 0;
   while (i < TEXT_BUFFER - 1) {
      char character = get_char();
      if ((character == '\r') || (character == '\n')) {
         print("\n");
         break;
      } else if ((character == '\b') || (character == 127)) {
         if (i > 0) {
            print("\b \b");
            i--;
         }// if backspace
      } else {
         character_list[i++] = character;
         char echo[2] = {character, '\0'};
         print(echo);
      }// if special characters
      character_list[i] = '\0';
      return character_list;
   }// while
}// get_input

bool test_keyboard() {
   char character = get_char();
   if (character != NULL) {
      return 1;
   } else {
      return 0;
   }// if to test if character is null
}// test_keyboard
