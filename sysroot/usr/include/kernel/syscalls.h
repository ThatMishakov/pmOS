#ifndef KERNEL_SYSCALLS_H
#define KERNEL_SYSCALLS_H

#define PMOS_SYSCALL_INT              0xf8

#define SYSCALL_EXIT                   0
#define SYSCALL_GETPID                 1
#define SYSCALL_CREATE_PROCESS         2
#define SYSCALL_START_PROCESS          3
#define SYSCALL_INIT_STACK             4
#define SYSCALL_SET_PRIORITY           5
#define SYSCALL_SET_TASK_NAME          6 
#define SYSCALL_GET_LAPIC_ID           7
#define SYSCALL_CONFIGURE_SYSTEM       8

#define SYSCALL_GET_MSG_INFO           9
#define SYSCALL_GET_MESSAGE            10
#define SYSCALL_REQUEST_NAMED_PORT     11
#define SYSCALL_SEND_MSG_PORT          12
#define SYSCALL_CREATE_PORT            13
#define SYSCALL_SET_ATTR               14
#define SYSCALL_SET_INTERRUPT          15
#define SYSCALL_NAME_PORT              16
#define SYSCALL_GET_PORT_BY_NAME       17
#define SYSCALL_SET_LOG_PORT           18

#define SYSCALL_GET_PAGE_TABLE         19

#define SYSCALL_TRANSFER_REGION        21
#define SYSCALL_CREATE_NORMAL_REGION   22
#define SYSCALL_GET_SEGMENT            23
#define SYSCALL_CREATE_PHYS_REGION     24
#define SYSCALL_DELETE_REGION          25
#define SYSCALL_UNMAP_RANGE            26
#define SYSCALL_MEM_PROTECT            27
#define SYSCALL_SET_SEGMENT            28
#define SYSCALL_ASIGN_PAGE_TABLE       29
#define SYSCALL_CREATE_MEM_OBJECT      30

#define SYSCALL_CREATE_TASK_GROUP      31
#define SYSCALL_ADD_TASK_TO_GROUP      32
#define SYSCALL_REMOVE_TASK_FROM_GROUP 33
#define SYSCALL_CHECK_IF_TASK_IN_GROUP 34



#endif