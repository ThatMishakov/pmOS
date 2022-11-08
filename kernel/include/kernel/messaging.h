#ifndef KERNEL_MESSAGING_H
#define KERNEL_MESSAGING_H
#include "types.h"

typedef struct {
    u64 sender;
    u64 channel;
    u64 size;
} Message_Descriptor;

#define MSG_ARG_NOPOP 0x01

#endif