#ifndef KERNEL_SYSCALLS_H
#define KERNEL_SYSCALLS_H

#define PMOS_SYSCALL_INT              0xf8

#define SYSCALL_GETPID                0x01
#define SYSCALL_CREATE_NORMAL_REGION  0x02
#define SYSCALL_CREATE_MANAGED_REGION 0x03
#define SYSCALL_CREATE_PHYS_REGION    0x04
#define SYSCALL_TRANSFER_REGION       0x05
#define SYSCALL_CREATE_PROCESS        0x07
#define SYSCALL_GET_PAGE_TABLE        0x08
#define SYSCALL_PROVIDE_PAGE          0x09
#define SYSCALL_START_PROCESS         0x0b
#define SYSCALL_EXIT                  0x0c
#define SYSCALL_GET_MSG_INFO          0x0e
#define SYSCALL_GET_MESSAGE           0x0f
#define SYSCALL_REQUEST_NAMED_PORT    0x10
#define SYSCALL_SEND_MSG_PORT         0x11
#define SYSCALL_SET_PORT              0x12
#define SYSCALL_SET_ATTR              0x15
#define SYSCALL_INIT_STACK            0x16
#define SYSCALL_CONFIGURE_SYSTEM      0x18
#define SYSCALL_SET_PRIORITY          0x1a
#define SYSCALL_GET_LAPIC_ID          0x1b
#define SYSCALL_SET_TASK_NAME         0x1c
#define SYSCALL_CREATE_PORT           0x1d
#define SYSCALL_SET_INTERRUPT         0x1e
#define SYSCALL_NAME_PORT             0x1f
#define SYSCALL_GET_PORT_BY_NAME      0x20
#define SYSCALL_SET_LOG_PORT          0x21
#define SYSCALL_SET_SEGMENT           0x22


#endif