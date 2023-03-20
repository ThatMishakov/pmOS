#ifndef _PMOS_HELPERS_H
#define _PMOS_HELPERS_H

#include <kernel/messaging.h>
#include "system.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef uint64_t pmos_port_t;

#ifdef __STDC_HOSTED__

/**
 * @brief Blocks the process and upon the message reception, allocates memory with malloc(), and fills it with the content. Internally,
 *        calls get_message_info() and get_first_message()
 * 
 * @param desc Pointer to the memory location where Message_descriptor will be filled
 * @param message malloc'ed message. To avoid memory leaks, it should be free'd after no longer nedded
 * @param port Valid port, to which the callee should be the owner, from where to get the message
 * @return result of the execution
 */
result_t get_message(Message_Descriptor* desc, unsigned char** message, pmos_port_t port);

#endif


#if defined(__cplusplus)
} /* extern "C" */
#endif


#endif