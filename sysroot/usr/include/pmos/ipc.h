#ifndef _PMOS_IPC_H
#define _PMOS_IPC_H
#include <stdint.h>
#include "acpi.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct IPC_Generic_Msg
{
    uint32_t type;
} IPC_Generic_Msg;

#define IPC_MIN_SIZE (sizeof(IPC_Generic_Msg))
#define IPC_RIGHT_SIZE(type) (sizeof(type))
#define IPC_TYPE(ptr) (((IPC_Generic_Msg*)ptr)->type)

// Registers an interrupt handler for the process
#define IPC_Reg_Int_NUM 0x01
typedef struct IPC_Reg_Int {
    uint32_t type;
    uint32_t flags;
    uint32_t intno;
    uint32_t reserved;
    uint64_t dest_task;
    uint64_t dest_chan;
    uint64_t reply_chan;
} IPC_Reg_Int;
#define IPC_Reg_Int_FLAG_EXT_INTS 0x01

#define IPC_Reg_Int_Reply_NUM 0x02
typedef struct IPC_Reg_Int_Reply {
    uint32_t type;
    uint32_t flags;
    uint32_t status;
    uint32_t intno;
} IPC_Reg_Int_Reply;

#define IPC_Start_Timer_NUM 0x03
typedef struct IPC_Start_Timer {
    uint32_t type;
    uint64_t ms;
    uint64_t extra;
    uint64_t reply_channel;
} IPC_Start_Timer;

#define IPC_Timer_Ctrl_NUM 0x04
typedef struct IPC_Timer_Ctrl {
    uint32_t type;
    uint32_t flags;
    uint32_t cmd;
    uint64_t timer_id;
} IPC_Timer_Ctrl;

#define IPC_Timer_Reply_NUM 0x05
typedef struct IPC_Timer_Reply {
    uint32_t type;
    uint32_t status;
    uint64_t timer_id;
    uint64_t extra;
} IPC_Timer_Reply;
#define IPC_TIMER_TICK 0x01

#define IPC_Kernel_Interrupt_NUM 0x20
typedef struct IPC_Kernel_Interrupt {
    uint32_t type;
    uint32_t intno;
    uint32_t lapic_id;
} IPC_Kernel_Interrupt;



#define IPC_Write_Plain_NUM     0x40
typedef struct IPC_Write_Plain {
    uint32_t type;
    char data[0];
} IPC_Write_Plain;


#define IPC_ACPI_Request_RSDT_NUM 0x60
typedef struct IPC_ACPI_Request_RSDT {
    uint32_t type;
    uint64_t reply_channel;
} IPC_ACPI_Request_RSDT;

#define IPC_ACPI_RSDT_Reply_NUM 0x61
typedef struct IPC_ACPI_RSDT_Reply {
    uint32_t type;
    uint32_t result;
    ACPI_RSDP_descriptor *descriptor;
} IPC_ACPI_RSDT_Reply;


#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif