#ifndef _DEVICESD_MSGS_H
#define _DEVICESD_MSGS_H
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct DEVICESD_MSG_GENERIC
{
    uint32_t type;
} DEVICESD_MSG_GENERIC;

// Registers an interrupt handler for the process
#define DEVICESD_MESSAGE_REG_INT_T 0x01
typedef struct DEVICESD_MESSAGE_REG_INT {
    uint32_t type;
    uint32_t flags;
    uint32_t intno;
    uint32_t reserved;
    uint64_t dest_task;
    uint64_t dest_chan;
    uint64_t reply_chan;
} DEVICESD_MESSAGE_REG_INT;
#define MSG_REG_INT_FLAG_EXTERNAL_INTS 0x01

#define DEVICESD_MESSAGE_REG_INT_REPLY_T 0x02
typedef struct DEVICESD_MESSAGE_REG_INT_REPLY {
    uint32_t type;
    uint32_t status;
    uint32_t intno;
} DEVICESD_MESSAGE_REG_INT_REPLY;

#define DEVICESD_MESSAGE_TIMER_T 0x03
typedef struct DEVICESD_MESSAGE_TIMER {
    uint32_t type;
    uint64_t ms;
    uint64_t reply_channel;
} DEVICESD_MESSAGE_TIMER;

#define DEVICESD_MESSAGE_TIMER_CONTROL_T 0x04
typedef struct DEVICESD_MESSAGE_TIMER_CONTROL {
    uint32_t type;
    uint32_t cmd;
    uint64_t timer_id;
} DEVICESD_MESSAGE_TIMER_CONTROL;

#define DEVICESD_MESSAGE_TIMER_REPLY_T 0x05
typedef struct DEVICESD_MESSAGE_TIMER_REPLY {
    uint32_t type;
    uint32_t status;
    uint64_t timer_id;
} DEVICESD_MESSAGE_TIMER_REPLY;


#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif