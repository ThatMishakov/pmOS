#ifndef INTERRUPTS_H
#define INTERRUPTS_H
#include <stdint.h>

extern uint8_t int_vector;

void get_interrupt_number();

#endif