#ifndef __NEO_H
#define __NEO_H

#define COLOR_RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"

void read_file(FILE *choosen_file);
void error_text(char text[],bool if_new_line);
void success_text(char text[],bool if_new_line);
void info_text(char text[],bool if_new_line);
void info(char call_reason[]);

#endif
