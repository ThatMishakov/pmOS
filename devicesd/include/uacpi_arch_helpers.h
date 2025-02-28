#pragma once

#include <uacpi/platform/atomic.h>
#include <pmos/system.h>

syscall_r __pmos_syscall_set_attr(uint64_t pid, uint32_t attr, unsigned long value);

#define UACPI_ARCH_FLUSH_CPU_CACHE() __pmos_syscall_set_attr(0, 7, 0)

typedef unsigned long uacpi_cpu_flags;
typedef void *uacpi_thread_id;
#define UACPI_ATOMIC_LOAD_THREAD_ID(ptr) ((uacpi_thread_id)uacpi_atomic_load_ptr(ptr))
#define UACPI_ATOMIC_STORE_THREAD_ID(ptr, value) uacpi_atomic_store_ptr(ptr, value)
#define UACPI_THREAD_ID_NONE ((uacpi_thread_id)-1)