#ifndef __ERROR_H
#define __ERROR_H

#define handle_load_access_error() handle_store_access_error()

/* Only initialize for future */
// hardware
void handle_clock_error() {}// handle_clock_error
// software
void handle_umode_error() {}// handle_umode_error
void handle_load_page_error() {}// handle_load_page_error
void handle_store_page_error() {}// handle_store_page_error
void handle_instruction_page_error() {}// handle_instruction_page_error
void handle_breakpoint() {}// handle_breakpoint
#endif
