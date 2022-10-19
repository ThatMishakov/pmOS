#ifndef KERNEL_SYSCALLS_H
#define KERNEL_SYSCALLS_H

#define PMOS_SYSCALL_INT        0xca

#define SYSCALL_GETPID          0x01
#define SYSCALL_BLOCK           0x02
#define SYSCALL_GET_PAGE        0x03
#define SYSCALL_GET_PAGE_MULTI  0x04
#define SYSCALL_RELEASE_PAGE    0x05
#define SYSCALL_CREATE_PROCESS  0x06
#define SYSCALL_MAP_INTO        0x07
#define SYSCALL_MAP_INTO_RANGE  0x08
#define SYSCALL_START_PROCESS   0x09
#define SYSCALL_EXIT            0x0a

#endif