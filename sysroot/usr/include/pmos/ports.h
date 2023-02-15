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

#ifdef __STDC_HOSTED__

// Creates a new port
ports_request_t create_port(pid_t owner, uint64_t flags);

// Assigns a name to the port
result_t name_port(pmos_port_t portnum, const char* name, size_t length, u32 flags);

// Requests a port by its name. This syscall blocks the process untill the name is created
ports_request_t get_port_by_name(const char *name, u64 length, u32 flags);

#endif

// Causes process not to block when the port doesn't exist, returning ERROR_PORT_DOESNT_EXIST instead 
#define FLAG_NOBLOCK    0x01



#if defined(__cplusplus)
}
#endif

#endif