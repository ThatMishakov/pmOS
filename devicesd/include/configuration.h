#ifndef CONFIGURATION_H
#define CONFIGURATION_H
#include <stdbool.h>
#include <pmos/ipc.h>
#include <stdint.h>

// Returns CPU int vector or 0 on error
uint8_t get_ioapic_int(uint32_t intno, uint64_t dest_pid, uint32_t chan);

// Returns true if was susccessful
uint8_t configure_interrupts_for(IPC_Reg_Int* desc);

struct interrupt_descriptor {
    uint32_t intno;
    uint8_t cpu_vector;
};

struct int_task_descriptor {
    uint64_t pid;
    uint64_t channel;
};

#endif