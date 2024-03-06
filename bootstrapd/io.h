#ifndef IO_H
#define IO_H
#include <stdint.h>

void print_str(char * str);
void int_to_hex(char * buffer, uint64_t n, char upper);
void print_hex(uint64_t i);
void set_print_syscalls(uint64_t port);
void print_str_n(char * str, int length);


#endif