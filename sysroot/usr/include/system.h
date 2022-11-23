#ifndef _SYSTEM_H
#define _SYSTEM_H 1
#include <stdint.h>
#include <stddef.h>
#include "kernel/syscalls.h"
#include "kernel/memory.h"
#include "kernel/messaging.h"
#include "kernel/types.h"

typedef uint64_t result_t;

typedef struct {
    result_t result;
    uint64_t value;
} syscall_r;

#if defined(__cplusplus)
extern "C" {
#endif

// Generic syscall
syscall_r syscall(uint64_t call_n, ...);

// Returns a pid of the process
u64 getpid();

// Allocates a page at *addr*
syscall_r syscall_get_page(uint64_t addr);

syscall_r map_into_range(uint64_t pid, uint64_t page_start, uint64_t to_addr, uint64_t nb_pages, uint64_t mask);

syscall_r syscall_new_process(uint8_t ring);

syscall_r get_page_multi(uint64_t base, uint64_t nb_pages);

// Releases multiple pages
result_t syscall_release_page_multi(u64 base, u64 nb_pages);

syscall_r start_process(uint64_t pid, uint64_t entry);

syscall_r map_phys(uint64_t virt, uint64_t phys, uint64_t size, uint64_t arg);

// Get info about message
result_t syscall_get_message_info(Message_Descriptor* descr);

// Gets first message in the messaging queue
result_t get_first_message(char* buff, uint64_t args);

// Sends a message to the process pid at channel *channel*
result_t send_message_task(uint64_t pid, uint64_t channel, size_t size, char* message);

// Sends a message to the port
result_t send_message_port(uint64_t port, size_t size, char* message);

// Sets the port of the process
result_t set_port(uint64_t pid, uint64_t dest_pid, uint64_t dest_chan);

// Sets default port
result_t set_port_default(uint64_t port, uint64_t dest_pid, uint64_t dest_chan);

// Blocks the process with the mask *mask*. Returns unblock reason as a value
syscall_r block(uint64_t mask);

#if defined(__cplusplus)
} /* extern "C" */
#endif


#endif