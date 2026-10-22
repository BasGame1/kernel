#include <stdint.h>
#include <stdbool.h>

#include "../include/userspace.h"
#include "../include/hardware.h"
#include "../include/keyboard.h"
#include "../include/gid.h"

#if defined(__X86__) || defined(__i386__) || defined(__x86_64__)
#include "../include/arch_x86.h"
#endif

char get_char() {
   volatile char input;
#if defined(__X86__) || defined(__i386__) || defined(__x86_64__)
   while ((inb(COM1_PORT + 5) & 0x01) == 0) {
   }// while
   input = (char)inb(COM1_PORT);
#else
   volatile uint8_t *uart_status_pointer = (volatile uint8_t *)UART_STATUS;
   volatile uint8_t *screen_address_pointer = (volatile uint8_t *)SCREEN_ADDRESS;
   while ((*uart_status_pointer & 0x01) == 0) {
   }// while
   input = (char)*screen_address_pointer;
#endif
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
         }// if
      } else {
         char echo[2] = {character, '\0'};
         print(echo);
         i++;
      }// if
   }// while
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
         }// if
      } else {
         character_list[i++] = character;
         char echo[2] = {character, '\0'};
         print(echo);
      }// if
   }// while
   character_list[i] = '\0';
   return character_list;
}// get_input

bool test_keyboard() {
#if defined(__X86__) || defined(__i386__) || defined(__x86_64__)
   return (inb(COM1_PORT + 5) != 0xFF);
#else
   volatile uint8_t *uart_status_pointer = (volatile uint8_t *)UART_STATUS;
   return (*uart_status_pointer != 0xFF);
#endif
}// test_keyboard

void keyboard_driver_process(void) {
   static char reply[2] = {0, 0};
   while (1) {
      char req = 0;
      uint32_t sender = user_ipc_recv(0, &req);
      if (sender != (uint32_t)-1) {
         char c = get_char();
         reply[0] = c;
         reply[1] = '\0';
         user_ipc_send(sender, reply);
      }// if
   }// while
}// keyboard_driver_process

void create_keyboard_process() {
   infon("Starting keyboard process");
   uint32_t pid = create_process((void *)keyboard_driver_process, KEYBOARD_GID);
   struct process *keyboard_process = get_process_by_pid(pid);
   if (keyboard_process) {
      uint32_t gid = keyboard_process->gid;
      uint32_t sid = generate_sid(gid, (uintptr_t)keyboard_driver_process);
      keyboard_process->sid = sid;
   }// if
   pkcall_map_device(SCREEN_ADDRESS, SCREEN_ADDRESS);
   pkcall_map_device(UART_STATUS, UART_STATUS);
   successn("keyboard process started");
}// create_keyboard_process
