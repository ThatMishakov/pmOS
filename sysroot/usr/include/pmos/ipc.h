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
    // Type of the message (must be IPC_Start_Timer_NUM)
    uint32_t type;

    // Time in milliseconds after which the reply will be sent
    uint64_t ms;

    // Port for the reply
    uint64_t reply_port;

    // Data up to the choosing of the sender, will be maintained in the reply message
    uint64_t extra0;
    uint64_t extra1;
    uint64_t extra2;
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
    uint64_t extra0;
    uint64_t extra1;
    uint64_t extra2;
} IPC_Timer_Reply;
#define IPC_TIMER_TICK 0x01

#define IPC_Kernel_Interrupt_NUM 0x20
typedef struct IPC_Kernel_Interrupt {
    uint32_t type;
    uint32_t intno;
    uint32_t lapic_id;
} IPC_Kernel_Interrupt;

typedef uint64_t pmos_port_t;

#define IPC_Kernel_Named_Port_Notification_NUM 0x21
typedef struct IPC_Kernel_Named_Port_Notification {
    uint32_t type;
    uint32_t reserved;
    pmos_port_t port_num;
    char port_name[0];
} IPC_Kernel_Named_Port_Notification;

#define IPC_Kernel_Alloc_Page_NUM 0x22
typedef struct IPC_Kernel_Alloc_Page {
    uint32_t type;
    uint32_t flags;
    uint64_t page_table_id;
    uint64_t page_addr;
} IPC_Kernel_Alloc_Page;

#define IPC_Kernel_Request_Page_NUM 0x23
/// Page request for Memory Object
typedef struct IPC_Kernel_Request_Page {
    uint32_t type;
    uint32_t flags;
    uint64_t mem_object_id;
    uint64_t page_offset;
} IPC_Kernel_Request_Page;


#define IPC_Write_Plain_NUM     0x40
typedef struct IPC_Write_Plain {
    uint32_t type;
    char data[0];
} IPC_Write_Plain;

#define IPC_Write_NUM 0x41
typedef struct IPC_Write {
    /// Message type (must be IPC_Write_NUM)
    uint32_t type;

    /// Flags changing the behaviour
    uint32_t flags;

    /// Specific identificator for the file within the process
    uint64_t file_id;

    /// Offset where the data should be written
    uint64_t offset;

    /// Channel where the reply would be sent
    uint64_t reply_chan;

    /// Data to be written
    char data[0];
};

#define IPC_Read_NUM 0x42
typedef struct IPC_Read {
    /// Message type (must be IPC_Read_NUM)
    uint32_t type;

    /// Flags changing the behaviour
    uint32_t flags;

    /// Specific identificator for the file within the process
    uint64_t file_id;

    /// Beginning of the file to be read
    uint64_t start_offset;

    /// Maximum size to be read
    uint64_t max_size;

    /// Channel where the reply would be sent
    uint64_t reply_chan;
} IPC_Read;

#define IPC_Read_Reply_NUM 0x50
typedef struct IPC_Read_Reply {
    /// Message type (must be IPC_Read_Reply_NUM)
    uint32_t num;

    /// Flags changing the behaviour
    uint16_t flags;

    /// Result of the operation
    uint16_t result_code;

    /// Data that was read. The size can be deduced from the size of the message.
    unsigned char data[0];
} IPC_Read_Reply;

#define IPC_Write_Reply_NUM 0x51
typedef struct IPC_Write_Reply {
    /// Message type (must be IPC_Write_Reply_NUM)
    uint32_t num;

    uint16_t flags;

    int16_t result;

    uint64_t bytes_written;
};


#define IPC_Open_NUM 0x58
typedef struct IPC_Open {
    /// Message type (must be IPC_Open_NUM)
    uint32_t num;

    /// Flags changing the behavior of the open operation
    uint32_t flags;

    /// Port where the reply will be sent
    pmos_port_t reply_port;

    /// Path of the file to be opened (flexible array member)
    char path[];
} IPC_Open;

#define IPC_Open_Reply_NUM 0x59
typedef struct IPC_Open_Reply {
    /// Message type (must be IPC_Open_Reply_NUM)
    uint32_t num;

    /// Result code indicating the outcome of the open operation
    int16_t result_code;

    /// Flags associated with the file system
    uint16_t fs_flags;

    /// ID of the file system
    uint64_t filesystem_id;

    /// Port associated with the file system
    pmos_port_t fs_port;
} IPC_Open_Reply;



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


// Structure used for registering a new PS/2 port by its driver
#define IPC_PS2_Reg_Port_NUM 0x80
typedef struct IPC_PS2_Reg_Port
{
    // Message type (must be equal to IPC_PS2_Reg_Port_NUM)
    uint32_t type;

    // Configuration flags
    uint32_t flags;

    // Internal ID of the port, decided by its driver (used for identification when driver registers more than one port)
    uint64_t internal_id;

    // Port used for sending commands to the port
    uint64_t cmd_port;

    // Port used for configuration
    uint64_t config_port;
} IPC_PS2_Reg_Port;

// Structure that can be sent by both driver and PS2d for internal configuration
#define IPC_PS2_Config_NUM 0x81
typedef struct IPC_PS2_Config {
    // Message type (must be equal to IPC_PS2_Config_NUM)
    uint32_t type;

    // Configuration flags
    uint32_t flags;

    // Internal ID of the port
    uint64_t internal_id;

    // Request type
    uint64_t request_type;

    // Result or command
    uint64_t result_cmd;
} IPC_PS2_Config;
#define IPC_PS2_Config_Reg_Port   1 // Indicates the chanel where ps/2 driver should send the data and requests or 0 on failure

// Structure used for communicating new data recieved by PS/2 drivers to PS2d
#define IPC_PS2_Notify_Data_NUM 0x82
typedef struct IPC_PS2_Notify_Data {
    // Message type (must be equal to IPC_PS2_Notify_Data_NUM)
    uint32_t type;

    // Configuration flags
    uint32_t flags;

    // Internal ID of the port, must be the same as used with IPC_PS2_Reg_Port
    uint64_t internal_id;

    // Data recieved by the port; size of the array should be determined by the message size
    char data[0];
} IPC_PS2_Notify_Data;

// Structure used by PS2d to send data to devices
#define IPC_PS2_Send_Data_NUM 0x83
typedef struct IPC_PS2_Send_Data {
    // Message type (must be equal to IPC_PS2_Send_Data_NUM)
    uint32_t type;

    // Configuration flags
    uint32_t flags;
    
    // Internal ID of the port
    uint64_t internal_id;

    // Data to be sent to the port; size of the array should be determined by the message size
    char data[0];
} IPC_PS2_Send_Data;


#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif