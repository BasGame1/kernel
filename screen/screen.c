#include <stdint.h>

#include "../include/userspace.h"
#include "../include/hardware.h"
#include "../include/gid.h"

void print_character(char *message) {
   if (__builtin_expect(!message, 0)) {
      return;
   }// if
   volatile uint8_t *uart_status_pointer = (volatile uint8_t *)UART_STATUS;
   volatile uint8_t *screen_address_pointer = (volatile uint8_t *)SCREEN_ADDRESS;
   const char *p = message;
   while (*p != '\0') {
      while ((*uart_status_pointer & 0x20) == 0) {
      }// while
      char ch = *p++;
      if (__builtin_expect(ch == '\010' || ch == '\177', 0)) {
         *screen_address_pointer = (char)'\010';
      } else {
         *screen_address_pointer = ch;
      }// if
   }// while
}// print_character

void clear_screen() {
   print_character("\033[2J\033[H");
}// clear_screen

void screen_driver_process() {
   static char ack_buf[64] = {0};
   while (1) {
      char msg_buf[64];
      uint32_t sender = user_ipc_recv(0, msg_buf);
      if (sender != (uint32_t)-1) {
         msg_buf[sizeof(msg_buf) - 1] = '\0';
         print_character(msg_buf);
         user_ipc_send(sender, ack_buf);
      }// if
   }// while
}// screen_driver_process

void create_screen_process() {
   infon("Starting screen process");
   uint32_t pid = create_process((void *)screen_driver_process, SCREEN_GID);
   struct process *screen_process = get_process_by_pid(pid);
   if (screen_process) {
      uint32_t gid = screen_process->gid;
      uint32_t sid = generate_sid(gid, (uintptr_t)screen_driver_process);
      screen_process->sid = sid;
   }// if
   pkcall_map_device(SCREEN_ADDRESS, SCREEN_ADDRESS);
   pkcall_map_device(UART_STATUS, UART_STATUS);
   successn("Screen process started");
}// create_screen_process

void start_screen() {
   clear_screen();
   print_asccii_art();
}// start_screen
