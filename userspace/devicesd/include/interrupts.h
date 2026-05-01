#ifndef INTERRUPTS_H
#define INTERRUPTS_H
#include <stdint.h>
#include <pmos/system.h>
#include <stdbool.h>

typedef struct {
    uint32_t source;
    uint32_t destination;
    uint8_t active_low;
    uint8_t level_trig;
} int_redirect_descriptor;

int_redirect_descriptor isa_gsi_mapping(uint32_t intno);

right_request_t set_up_gsi(uint32_t gsi, bool active_low, bool level_trigger, pmos_port_t reply_port);
right_request_t install_isa_interrupt(uint32_t isa_pin, pmos_port_t reply_port);

#endif