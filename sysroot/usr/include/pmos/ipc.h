/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

    // Status of the operation. Negative values indicate errors (errno of negative sign)
    int32_t status;

    uint64_t timer_id;
    uint64_t extra0;
    uint64_t extra1;
    uint64_t extra2;
} IPC_Timer_Reply;
#define IPC_TIMER_TICK 0x01

#define IPC_Request_Serial_NUM 0x06
typedef struct IPC_Request_Serial {
    uint32_t type;
    uint32_t flags;
    uint64_t reply_port;
    uint64_t serial_id;
} IPC_Request_Serial;

#define IPC_Serial_Reply_NUM 0x07
// TODO: Document this
// See devicesd/include/serial.h for details
typedef struct IPC_Serial_Reply {
    uint32_t type;
    uint32_t flags;
    // 0 on success, -errno on failure
    int32_t result;

    uint64_t base_address;
    uint8_t access_type;
    uint8_t access_width;
    uint8_t interface_type;
    uint8_t pc_int_number;
    uint32_t baud_rate;
    uint8_t parity;
    uint8_t stop_bits;
    uint8_t flow_control;
    uint8_t terminal_type;
    uint32_t gsi_number;
    uint8_t interrupt_type;
    uint8_t reserved[7];
} IPC_Serial_Reply;

#define IPC_Kernel_Interrupt_NUM 0x20
typedef struct IPC_Kernel_Interrupt {
    uint32_t type;
    uint32_t intno;
    uint32_t cpu_id;
} IPC_Kernel_Interrupt;

typedef uint64_t pmos_port_t;

#define IPC_Kernel_Named_Port_Notification_NUM 0x21
typedef struct IPC_Kernel_Named_Port_Notification {
    uint32_t type;
    uint32_t reserved;
    pmos_port_t port_num;
    char port_name[0];
} IPC_Kernel_Named_Port_Notification;

#define IPC_Kernel_Request_Page_NUM 0x23
/// Page request for Memory Object
typedef struct IPC_Kernel_Request_Page {
    uint32_t type;
    uint32_t flags;
    uint64_t mem_object_id;
    uint64_t page_offset;
} IPC_Kernel_Request_Page;

#define IPC_Kernel_Group_Destroyed_NUM 0x24
/// Notification of port group destruction
typedef struct IPC_Kernel_Group_Destroyed {
    /// @brief  Message type (must be IPC_Kernel_Group_Destroyed_NUM)
    uint32_t type;

    /// Flags
    uint32_t flags;

    /// ID of the port group that had been destroyed
    uint64_t port_group_id;
} IPC_Kernel_Group_Destroyed;

/// Notification of task being removed from a group
#define Event_Group_Task_Removed_NUM 0x01
/// Notification of task being added to a group
#define Event_Group_Task_Added_NUM 0x02

#define IPC_Kernel_Group_Task_Changed_NUM 0x25
/// Notification of task group change of tasks
typedef struct IPC_Kernel_Group_Task_Changed {
    /// @brief  Message type (must be IPC_Kernel_Group_Task_Changed_NUM)
    uint32_t type;

    /// Flags
    uint16_t flags;

    /// Event type
    uint16_t event_type;

    /// ID of the task group that had been changed
    uint64_t task_group_id;

    /// ID of the task that has changed
    uint64_t task_id;
} IPC_Kernel_Group_Task_Changed;


#define IPC_Write_Plain_NUM     0x40
typedef struct IPC_Write_Plain {
    uint32_t type;
    char data[0];
} IPC_Write_Plain;

/// Registers a task with the VFS daemon in case it is not already
#define IPC_FLAG_REGISTER_IF_NOT_FOUND 0x01

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

    /// ID of the filesystem consumer
    uint64_t fs_consumer_id;

    /// Beginning of the file to be read
    uint64_t start_offset;

    /// Maximum size to be read
    uint64_t max_size;

    /// Channel where the reply would be sent
    uint64_t reply_port;
} IPC_Read;

#define IPC_Read_Reply_NUM 0x50
typedef struct IPC_Read_Reply {
    /// Message type (must be IPC_Read_Reply_NUM)
    uint32_t type;

    /// Flags changing the behaviour
    uint16_t flags;

    /// Result of the operation
    int16_t result_code;

    /// Data that was read. The size can be deduced from the size of the message.
    unsigned char data[];
} IPC_Read_Reply;

#define IPC_Write_Reply_NUM 0x51
typedef struct IPC_Write_Reply {
    /// Message type (must be IPC_Write_Reply_NUM)
    uint32_t num;

    uint16_t flags;

    int16_t result;

    uint64_t bytes_written;
};

#define IPC_Stat_NUM 0x56
/// Message sent by the user process to VFS daemon get file stats
typedef struct IPC_Stat {
    /// Message type (must be IPC_Stat_NUMº)
    uint32_t num;

    /// Flags changing the behavior of the open operation
    uint32_t flags;

    /// Port where the reply will be sent
    pmos_port_t reply_port;

    /// ID of the file system consumer
    uint64_t fs_consumer_id;

    /// Path of the file
    char path[];
} IPC_Stat;

#define IPC_Stat_Reply_NUM 0x57
/// Reply to the IPC_Stat message from the vfs daemon
typedef struct IPC_Stat_Reply {
    /// Message type (must be IPC_Stat_Reply_NUM)
    uint32_t num;

    /// Flags
    uint16_t flags;

    /// Result code (0 on success, -errno otherwise)
    int16_t result;

    /// Device ID
    uint64_t st_dev;

    /// File serial number
    uint64_t ino_t;

    /// Mode of the file
    uint64_t st_mode;

    /// Number of hard links to the file
    uint64_t st_nlink;

    /// User ID of the file
    uint64_t st_uid;

    /// Group ID of the file
    uint64_t st_gid;

    /// Device ID
    uint64_t st_rdev;

    /// File size
    uint64_t st_size;

    /// Time of last access
    uint64_t st_atim_tv_sec;
    uint64_t st_atim_tv_nsec;

    /// Time of last modification
    uint64_t st_mtim_tv_sec;
    uint64_t st_mtim_tv_nsec;

    /// Time of last status change
    uint64_t st_ctim_tv_sec;
    uint64_t st_ctim_tv_nsec;

    /// Block size
    uint64_t st_blksize;
    
    /// Number of blocks allocated
    uint64_t st_blocks;
} IPC_Stat_Reply;

#define IPC_Open_NUM 0x58
/// Message sent by the user process to VFS daemon to open a file
typedef struct IPC_Open {
    /// Message type (must be IPC_Open_NUM)
    uint32_t num;

    /// Flags changing the behavior of the open operation
    uint32_t flags;

    /// Port where the reply will be sent
    pmos_port_t reply_port;

    /// ID of the file system consumer
    uint64_t fs_consumer_id;

    /// Path of the file to be opened (flexible array member)
    char path[];
} IPC_Open;

#define IPC_Open_Reply_NUM 0x59
typedef struct IPC_Open_Reply {
    /// Message type (must be IPC_Open_Reply_NUM)
    uint32_t type;

    /// Result code indicating the outcome of the open operation
    int16_t result_code;

    /// Flags associated with the file system
    uint16_t fs_flags;

    /// ID of the file system
    uint64_t filesystem_id;

    /// ID of the file
    uint64_t file_id;

    /// Port associated with the file system
    pmos_port_t fs_port;
} IPC_Open_Reply;

#define IPC_Dup_NUM 0x5d
typedef struct IPC_Dup {
    /// Message type (must be IPC_Dup_NUM)
    uint32_t type;

    /// Flags changing the behaviour
    uint32_t flags;

    /// ID of the file to be duplicated
    uint64_t file_id;

    /// ID of the filesystem
    uint64_t filesystem_id;

    /// ID of the consumer that has the file currently opened
    uint64_t fs_consumer_id;

    /// ID of the consumer that will have the file opened
    uint64_t new_consumer_id;

    /// Port where the reply would be sent
    uint64_t reply_port;
} IPC_Dup;

#define IPC_Dup_Reply_NUM 0x5e
typedef struct IPC_Dup_Reply {
    /// Message type (must be IPC_Dup_Reply_NUM)
    uint32_t type;

    /// Result of the operation
    int32_t result_code;

    /// ID of the file
    uint64_t file_id;

    /// ID of the filesystem
    uint64_t filesystem_id;

    /// Port associated with the file
    pmos_port_t fs_port;
} IPC_Dup_Reply;

#define IPC_Create_Consumer_NUM 0x5a
typedef struct IPC_Create_Consumer {
    /// Message type (must be IPC_Create_Consumer_NUM)
    uint32_t type;

    /// Flags changing the behaviour
    uint32_t flags;

    /// Port where the reply would be sent
    uint64_t reply_port;

    /// ID of the tasks group representing the consumer
    uint64_t task_group_id;
} IPC_Create_Consumer;

#define IPC_Create_Consumer_Reply_NUM 0x5b
typedef struct IPC_Create_Consumer_Reply {
    /// Message type (must be IPC_Create_Consumer_Reply_NUM)
    uint32_t type;

    /// Flags changing the behaviour
    uint32_t flags;

    /// Result of the operation
    int32_t result_code;
} IPC_Create_Consumer_Reply;

#define IPC_Close_NUM 0x5c
/// Message sent by the user process to VFS daemon to close a file
typedef struct IPC_Close {
    /// Message type (must be IPC_Create_Consumer_Reply_NUM)
    uint32_t type;

    /// Flags changing the behaviour
    uint32_t flags;

    /// ID of the file system consumer
    uint64_t fs_consumer_id;

    /// ID of the file system
    uint64_t filesystem_id;

    /// ID of the file
    uint64_t file_id;
} IPC_Close;



#define IPC_ACPI_Request_RSDT_NUM 0x60
typedef struct IPC_ACPI_Request_RSDT {
    uint32_t type;
    uint64_t reply_channel;
} IPC_ACPI_Request_RSDT;

#define IPC_ACPI_RSDT_Reply_NUM 0x61
typedef struct IPC_ACPI_RSDT_Reply {
    uint32_t type;
    int32_t result;
    ACPI_RSDP_descriptor *descriptor;
} IPC_ACPI_RSDT_Reply;

#define IPC_FDT_Request_NUM 0x62
typedef struct IPC_FDT_Request {
    uint32_t type;
    uint64_t reply_channel;
} IPC_FDT_Request;

#define IPC_FDT_Reply_NUM 0x63
typedef struct IPC_FDT_Reply {
    uint32_t type;
    int32_t result;
    uint64_t fdt_mem_object_id;
    uint32_t object_offset;
    uint32_t object_size;
} IPC_FDT_Reply;


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

#define IPC_Mutex_Unlock_NUM 0xA0
typedef struct IPC_Mutex_Unlock {
    /// Message type (must be IPC_Mutex_Unlock_NUM)
    uint32_t type;

    /// Flags changing the behaviour
    uint32_t flags;
} IPC_Mutex_Unlock;

#define IPC_FS_Open_NUM 0xC0
typedef struct IPC_FS_Open {
    /// Message type (must be IPC_FS_Open_NUM)
    uint32_t type;

    /// Flags changing the behaviour
    uint32_t flags;

    /// Port where the reply will be sent
    pmos_port_t reply_port;

    /// ID of the file system consumer
    uint64_t fs_consumer_id;

    /// ID of the file
    uint64_t file_id;

    /// Operation ID
    uint64_t operation_id;
} IPC_FS_Open;

#define IPC_Register_FS_NUM 0xC1
/// Message sent by a filesystem driver to register itself with VFS daemon
typedef struct IPC_Register_FS {
    /// Message type (must be IPC_REGISTER_FS_NUM)
    uint32_t num;

    /// Flags indicating the behavior of the registration
    uint32_t flags;

    /// Port where the reply will be sent
    pmos_port_t reply_port;

    /// Port associated with the filesystem
    pmos_port_t fs_port;

    /// Additional data specific to the filesystem (flexible array member)
    char name[];
} IPC_Register_FS;

#define IPC_Mount_FS_NUM 0xC2
/// Message sent by a user process to VFS daemon to mount a filesystem
typedef struct IPC_Mount_FS {
    /// Message type (must be IPC_Mount_FS_NUM)
    uint32_t num;

    /// Flags indicating the behavior of the mount operation
    uint32_t flags;

    /// Port where the reply will be sent
    pmos_port_t reply_port;

    /// ID of the filesystem to be mounted
    uint64_t filesystem_id;

    /// Root file descriptor
    uint64_t root_fd;

    /// Path where the filesystem should be mounted (flexible array member)
    char mount_path[];
} IPC_Mount_FS;

#define IPC_UNMOUNT_FS_NUM 0xC3
/// Message sent by a user process to VFS daemon to unmount a filesystem
typedef struct IPC_Unmount_FS {
    /// Message type (must be IPC_UNMOUNT_FS_NUM)
    uint32_t num;

    /// Flags indicating the behavior of the unmount operation
    uint32_t flags;

    /// Port where the reply will be sent
    pmos_port_t reply_port;

    /// ID of the filesystem
    uint64_t filesystem_id;

    /// ID of the mountpoint to be unmounted
    uint64_t mountpoint_id;
} IPC_Unmount_FS;

#define IPC_FS_Resolve_Path_NUM 0xC4
typedef struct IPC_FS_Resolve_Path {
    /// Message type (must be IPC_FS_Resolve_Path_NUM)
    uint32_t type;

    /// Flags changing the behaviour
    uint32_t flags;

    /// Port where the reply will be sent
    pmos_port_t reply_port;

    /// ID of the filesystem
    uint64_t filesystem_id;

    /// ID of the parent directory
    uint64_t parent_dir_id;

    /// Operation ID
    uint64_t operation_id;

    /// Name of the file node
    char path_name[];
} IPC_FS_Resolve_Path;

#define IPC_FS_Dup_NUM 0xC5
/// @brief Message sent by a user process to VFS daemon to duplicate a file.
///        This function is used by dup2() or similar functions where fs_consumer_id == new_consumer_id
///        and during the fork() operation or similar, where the file is reopened/duplicated into the
///        new filesystem consumer.
typedef struct IPC_FS_Dup {
    /// Message type (must be IPC_FS_Dup_NUM)
    uint32_t type;

    /// Flags changing the behaviour
    uint32_t flags;

    /// ID of the operation, must be the same in the request and reply
    uint64_t operation_id;

    /// ID of the file to be duplicated
    uint64_t file_id;

    /// ID of the filesystem
    uint64_t filesystem_id;

    /// ID of the consumer that has the file currently opened
    uint64_t fs_consumer_id;

    /// ID of the consumer that will have the file opened
    uint64_t new_consumer_id;

    /// Port where the reply would be sent
    uint64_t reply_port;
} IPC_FS_Dup;

#define IPC_FS_Close_NUM 0xC6
/// Message sent by a VFS daemon to a filesystem driver to close a file
typedef struct IPC_FS_Close {
    /// Message type (must be IPC_FS_Close_NUM)
    uint32_t type;

    /// Flags changing the behaviour
    uint32_t flags;

    /// ID of the filesystem
    uint64_t filesystem_id;

    /// ID of the file
    uint64_t file_id;
} IPC_FS_Close;

#define IPC_FS_Open_Reply_NUM 0xD0
typedef struct IPC_FS_Open_Reply {
    /// Message type (must be IPC_FS_OPEN_REPLY_NUM)
    uint32_t type;

    /// Result code indicating the outcome of the open operation
    int16_t result_code;

    /// Flags associated with the file system
    uint16_t fs_flags;

    /// Port associated with the file
    pmos_port_t file_port;

    /// ID of the file within the file system
    uint64_t file_id;

    /// Operation ID
    uint64_t operation_id;
} IPC_FS_Open_Reply;


#define IPC_Register_FS_Reply_NUM 0xD1
typedef struct IPC_Register_FS_Reply {
    /// Message type (must be IPC_REGISTER_FS_REPLY_NUM)
    uint32_t type;

    /// Result code indicating the outcome of the registration
    int32_t result_code;

    /// ID of the registered filesystem
    uint64_t filesystem_id;
} IPC_Register_FS_Reply;


#define IPC_Mount_FS_Reply_NUM 0xD2
typedef struct IPC_Mount_FS_Reply{
    /// Message type (must be IPC_MOUNT_FS_REPLY_NUM)
    uint32_t type;

    /// Result code indicating the outcome of the mount operation
    int32_t result_code;

    /// ID of the new mountpoint
    uint64_t mountpoint_id;
} IPC_Mount_FS_Reply;


#define IPC_UNMOUNT_FS_REPLY_NUM 0xD3
typedef struct IPC_Unmount_FS_Reply {
    /// Message type (must be IPC_UNMOUNT_FS_REPLY_NUM)
    uint32_t type;

    /// Result code indicating the outcome of the unmount operation
    int32_t result_code;
} IPC_Unmount_FS_Reply;


#define IPC_FS_Resolve_Path_Reply_NUM 0xD4
typedef struct IPC_FS_Resolve_Path_Reply {
    /// Message type (must be IPC_FS_Resolve_Path_Reply_NUM)
    uint32_t type;

    /// Flags
    uint16_t flags;

    /// Result code indicating the outcome of the operation
    int16_t result_code;

    /// Operation ID
    uint64_t operation_id;

    /// ID of the file node
    uint64_t file_id;

    /// File type
    uint16_t file_type;
} IPC_FS_Resolve_Path_Reply;

#define IPC_FS_Dup_Reply_NUM 0xD5
typedef struct IPC_FS_Dup_Reply {
    /// Message type (must be IPC_FS_Dup_Reply_NUM)
    uint32_t type;

    /// Result code indicating the outcome of the open operation
    int16_t result_code;

    /// Flags associated with the file system
    uint16_t fs_flags;

    /// Port associated with the file
    pmos_port_t file_port;

    /// ID of the file within the file system
    uint64_t file_id;

    /// Operation ID
    uint64_t operation_id;
} IPC_FS_Dup_Reply;

#define IPC_Pipe_Open_NUM 0xE0
typedef struct IPC_Pipe_Open {
    /// Message type (must be IPC_Pipe_Open_NUM)
    uint32_t type;

    /// Flags changing the behaviour
    uint32_t flags;

    /// Port where the reply will be sent
    pmos_port_t reply_port;

    /// ID of the file system consumer
    uint64_t fs_consumer_id;
} IPC_Pipe_Open;

#define IPC_Pipe_Open_Reply_NUM 0xE1
typedef struct IPC_Pipe_Open_Reply {
    /// Message type (must be IPC_Pipe_Open_Reply_NUM)
    uint32_t type;

    /// Flags
    uint16_t flags;

    /// Result code indicating the outcome of the open operation
    int16_t result_code;

    /// ID of the file system
    uint64_t filesystem_id;

    /// ID of the reader end of the pipe
    uint64_t reader_id;

    /// ID of the writer end of the pipe
    uint64_t writer_id;

    /// Port associated with the pipe
    pmos_port_t pipe_port;
} IPC_Pipe_Open_Reply;

#define IPC_Thread_Finished_NUM 0x100
/// @brief Message sent by the thread to the one calling pthread_join() when notifing that it has finished.
typedef struct IPC_Thread_Finished {
    /// Message type (must be IPC_Thread_Finished_NUM)
    uint32_t type;

    /// Flags
    uint32_t flags;

    /// ID of the thread that has finished
    uint64_t thread_id;
} IPC_Thread_Finished;



#define IPC_Framebuffer_Request_NUM 0x120
/// @brief Request of the framebuffer to the bootstrap server or devices driver
typedef struct IPC_Framebuffer_Request {
    /// Message type (must be IPC_Framebuffer_Request_NUM)
    uint32_t type;

    /// Flags
    uint32_t flags;

    /// Port for the reply
    pmos_port_t reply_port;
} IPC_Framebuffer_Request;

#define IPC_Framebuffer_Reply_NUM 0x121
/// @brief Reply with the framebuffer address
typedef struct IPC_Framebuffer_Reply {
    /// Message type (must be IPC_Framebuffer_Reply_NUM)
    uint32_t type;

    /// Flags
    uint16_t flags;

    /// Result code
    int64_t result_code;

    // Framebuffer data
    uint64_t framebuffer_addr;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_bpp;
    uint16_t framebuffer_memory_model;
    uint8_t framebuffer_red_mask_size;
    uint8_t framebuffer_red_mask_shift;
    uint8_t framebuffer_green_mask_size;
    uint8_t framebuffer_green_mask_shift;
    uint8_t framebuffer_blue_mask_size;
    uint8_t framebuffer_blue_mask_shift;
}  IPC_Framebuffer_Reply;

#define IPC_Register_Log_Output_NUM 0x140
typedef struct IPC_Register_Log_Output {
    /// Message type (must be IPC_Register_Log_Output_NUM)
    uint32_t type;

    /// Flags
    uint32_t flags;

    /// Port for the reply
    pmos_port_t reply_port;

    /// Log output port
    pmos_port_t log_port;

    /// ID of the task that wants to register the log output
    uint64_t task_id;
} IPC_Register_Log_Output;

#define IPC_Log_Output_Reply_NUM 0x141
typedef struct IPC_Log_Output_Reply {
    /// Message type (must be IPC_Log_Output_Reply_NUM)
    uint32_t type;

    /// Flags
    uint32_t flags;

    /// Result code
    int64_t result_code;
} IPC_Log_Output_Reply;

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif