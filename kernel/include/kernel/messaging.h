#ifndef KERNEL_MESSAGING_H
#define KERNEL_MESSAGING_H
#include <stdint.h>

typedef struct {
    uint64_t sender;
    uint64_t channel;
    uint64_t size;
} Message_Descriptor;

#define MSG_ARG_NOPOP 0x01

#endif