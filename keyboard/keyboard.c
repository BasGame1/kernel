#include <stdint.h>
#include <stdbool.h>

#include "../include/userspace.h"
#include "../include/hardware.h"
#include "../include/keyboard.h"
#include "../include/gid.h"

char get_char() {
   volatile char input;
   volatile uint8_t *uart_status_pointer = (volatile uint8_t *) UART_STATUS;
   volatile uint8_t *screen_address_pointer = (volatile uint8_t *) SCREEN_ADDRESS;
   while ((*uart_status_pointer & 0x01) == 0) {
   }
   input = (char) *screen_address_pointer;
   return input;
}// get_char
    
void print_input() {
   uint32_t i = 0;
   while (i < BUFFER_SIZE) {
      char character = get_char();
      if ((character == '\r') || (character == '\n')) {
         print("\n");
         break;
      } else if ((character == '\b') || (character == 127)) {
         if (i > 0) {
            print("\b \b");
            i--;
         }
      } else {
         char echo[2] = {character, '\0'};
         print(echo);
         i++;
      }
   }
}// print_input

char *get_input() {
   static char character_list[BUFFER_SIZE];
   uint32_t i = 0;
   while (i < BUFFER_SIZE - 1) {
      char character = get_char();
      if ((character == '\r') || (character == '\n')) {
         print("\n");
         break;
      } else if ((character == '\b') || (character == 127)) {
         if (i > 0) {
            print("\b \b");
            i--;
         }
      } else {
         character_list[i++] = character;
         char echo[2] = {character, '\0'};
         print(echo);
      }
   }
   character_list[i] = '\0';
   return character_list;
}// get_input

bool test_keyboard() {
   volatile uint8_t *uart_status_pointer = (volatile uint8_t *) UART_STATUS;
   return (*uart_status_pointer != 0xFF);
}// test_keyboard

void create_keyboard_process() {
   printu("Starting keyboard process");
   uint32_t pid = create_process((void *)get_input, KEYBOARD_GID);
   struct process *keyboard_process = get_process_by_pid(pid);
   if (keyboard_process) {
      uint32_t gid = keyboard_process->gid;
      uint32_t sid = generate_sid(gid, (uintptr_t)get_input);
      keyboard_process->sid = sid;
   }// if
   pkcall_map_device(SCREEN_ADDRESS, SCREEN_ADDRESS);
   pkcall_map_device(UART_STATUS, UART_STATUS);
   printu("keyboard process started");
}// create_keyboard_process
