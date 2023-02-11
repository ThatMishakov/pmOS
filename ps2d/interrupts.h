#ifndef INTERRUPTS_H
#define INTERRUPTS_H
#include <stdint.h>

extern uint8_t int_vector;


uint8_t get_interrupt_number(uint32_t intnum, uint64_t int_chan);

#endif