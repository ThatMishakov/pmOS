#include <interrupts/interrupts.h>

unsigned char interrupt_number = 200;

uint8_t get_free_interrupt()
{
    return interrupt_number--;
}