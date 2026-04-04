#ifndef INTERRUPTS_H
#define INTERRUPTS_H
#include <stdint.h>
#include <pmos/system.h>
#include <stdbool.h>

int install_isa_interrupt(uint32_t isa_pin, uint64_t task, pmos_port_t port, uint32_t *vector);
int set_up_gsi(uint32_t gsi, bool active_low, bool level_trig, uint64_t task, pmos_port_t port, uint32_t *vector);
int register_interrupt(uint32_t cpu_id, uint32_t vector, uint64_t task, pmos_port_t port);

typedef struct {
    uint32_t source;
    uint32_t destination;
    uint8_t active_low;
    uint8_t level_trig;
} int_redirect_descriptor;

int_redirect_descriptor isa_gsi_mapping(uint32_t intno);

#endif