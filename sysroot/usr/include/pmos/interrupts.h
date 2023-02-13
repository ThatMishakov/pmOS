#ifndef _PMOS_INTERRUPTS_H
#define _PMOS_INTERRUPTS_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "ports.h"

result_t set_interrupt(pmos_port_t port, uint32_t intno, uint32_t flags);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* _PMOS_INTERRUPTS_H */