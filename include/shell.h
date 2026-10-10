#ifndef __SHELL_H
#define __SHELL_H

#include <stdint.h>
#include <stdbool.h>

void init_shell(void);
void wait_command(void);
int check_command(char *full_command);

void call_help(void);
void call_info(void);
void call_version(void);
void call_error(char *command);

int call_neo(int argc, char **argv);
int call_memset(int argc, char **argv);
int call_round(int argc, char **argv);

void ls(char *path);
void *memcpy(void *dest, const void *src, size_t n);
#endif
