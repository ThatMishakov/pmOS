#ifndef _PMOS_INTERRUPTS_H
#define _PMOS_INTERRUPTS_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "ports.h"

#ifdef __STDC_HOSTED__

result_t set_interrupt(pmos_port_t port, uint32_t intno, uint32_t flags);

#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* _PMOS_INTERRUPTS_H */