#ifndef INTERRUPTS_H
#define INTERRUPTS_H
#include <stdint.h>

extern uint8_t int_vector;
static const uint64_t interrupts_conf_reply_chan = 5;
static const uint64_t int1_chan  = 10;
static const uint64_t int12_chan = 11;


uint8_t get_interrupt_number(uint32_t intnum, uint64_t int_chan);

#endif