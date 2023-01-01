#ifndef CONFIGURATION_H
#define CONFIGURATION_H
#include <stdbool.h>
#include <devicesd/devicesd_msgs.h>
#include <stdint.h>

// Returns CPU int vector or 0 on error
uint8_t get_ioapic_int(uint32_t intno, uint64_t dest_pid, uint32_t chan);

// Returns true if was susccessful
uint8_t configure_interrupts_for(DEVICESD_MESSAGE_REG_INT* desc);

struct interrupt_descriptor {
    uint32_t intno;
    uint8_t cpu_vector;
};

struct int_task_descriptor {
    uint64_t pid;
    uint64_t channel;
};

#endif