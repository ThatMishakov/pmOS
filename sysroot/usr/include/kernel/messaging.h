#ifndef KERNEL_MESSAGING_H
#define KERNEL_MESSAGING_H
#include "types.h"

#define SYSTEM_MESSAGES_START (0x01UL << 31)

typedef struct {
    u64 sender;
    u64 channel;
    u64 size;
} Message_Descriptor;

#define MSG_ARG_NOPOP 0x01

typedef struct {
    u32 type;
} PACKED Kernel_Message;


#define IPC_Kernel_Interrupt_NUM       0x06
#define KERNEL_MSG_INTERRUPT           IPC_Kernel_Interrupt_NUM
#define KERNEL_MSG_INT_START       SYSTEM_MESSAGES_START
typedef struct Kernel_Message_Interrupt {
    u32 type;
    u32 intno;
    u32 lapic_id;
} PACKED Kernel_Message_Interrupt;

#endif