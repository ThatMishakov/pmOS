#ifndef _PMOS_PORTS_H
#define _PMOS_PORTS_H
#include "../kernel/syscalls.h"
#include "../kernel/types.h"
#include "system.h"
#include <stdint.h>
#include <unistd.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef PID_SELF
#define PID_SELF 0
#endif

typedef uint64_t pmos_port_t;

typedef struct ports_request_t
{
    result_t result;
    pmos_port_t port;
} ports_request_t;

ports_request_t create_port(pid_t owner, uint64_t flags);



#if defined(__cplusplus)
}
#endif

#endif