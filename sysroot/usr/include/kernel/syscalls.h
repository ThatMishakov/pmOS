#ifndef KERNEL_SYSCALLS_H
#define KERNEL_SYSCALLS_H

#define PMOS_SYSCALL_INT              0xca

#define SYSCALL_GETPID                0x01
#define SYSCALL_BLOCK                 0x02
#define SYSCALL_GET_PAGE              0x03
#define SYSCALL_GET_PAGE_MULTI        0x04
#define SYSCALL_RELEASE_PAGE          0x05
#define SYSCALL_RELEASE_PAGE_MULTI    0x06
#define SYSCALL_CREATE_PROCESS        0x07
#define SYSCALL_MAP_INTO              0x08
#define SYSCALL_MAP_INTO_RANGE        0x09
#define SYSCALL_SHARE_WITH_RANGE      0x0a
#define SYSCALL_START_PROCESS         0x0b
#define SYSCALL_EXIT                  0x0c
#define SYSCALL_MAP_PHYS              0x0d
#define SYSCALL_GET_MSG_INFO          0x0e
#define SYSCALL_GET_MESSAGE           0x0f
#define SYSCALL_SEND_MSG_TASK         0x10
#define SYSCALL_SEND_MSG_PORT         0x11
#define SYSCALL_SET_PORT              0x12
#define SYSCALL_SET_PORT_KERNEL       0x13
#define SYSCALL_SET_PORT_DEFAULT      0x14
#define SYSCALL_SET_ATTR              0x15
#define SYSCALL_INIT_STACK            0x16
#define SYSCALL_IS_PAGE_ALLOCATED     0x17
#define SYSCALL_CONFIGURE_SYSTEM      0x18
#define SYSCALL_REG_NOTIFY_LISTENER   0x19
#define SYSCALL_SET_PRIORITY          0x1a


#endif