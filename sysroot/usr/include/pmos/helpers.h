#ifndef _PMOS_HELPERS_H
#define _PMOS_HELPERS_H

#include <kernel/messaging.h>
#include "system.h"

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef __STDC_HOSTED__

// Gets message. Fills desc and allocates and gets message
result_t get_message(Message_Descriptor* desc, unsigned char** message);

#endif


#if defined(__cplusplus)
} /* extern "C" */
#endif


#endif