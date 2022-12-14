#ifndef INTS_OVERRIDE_H
#define INTS_OVERRIDE_H

#include <stdint.h>

typedef struct {
    uint32_t source;
    uint32_t destination;
    uint8_t active_low;
    uint8_t level_trig;
} int_redirect_descriptor;

void register_redirect(uint32_t source, uint32_t to, uint8_t active_low, uint8_t level_trig);
int_redirect_descriptor get_for_int(uint32_t intno);

#endif