#include <stdint.h>
#include <pmos/system.h>
#include <stdbool.h>
#include <stdio.h>
#include <vector.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

struct CPU {
    uint32_t allocated_interrupts;
};

VECTOR(struct CPU) cpus;

void init_cpus()
{
    int count = get_nprocs();
    assert(count > 0);

    for (int i = 0; i < count; ++i) {
        int r = 0;
        struct CPU d = {
            0,
        };

        VECTOR_PUSH_BACK_CHECKED(cpus, d, r);
        if (r != 0) {
            fprintf(stderr, "Warning: Could not push CPU to vector: %i (%s)\n", errno, strerror(errno));
        }
    }
}

struct InterruptDescriptor {
    struct InterruptDescriptor *next;
    uint32_t gsi;
    uint32_t cpu;
};

struct InterruptDescriptor *interrupts = NULL;
struct InterruptDescriptor *find_descriptor(uint32_t gsi)
{
    struct InterruptDescriptor *desc = interrupts;
    while (desc) {
        if (desc->gsi == gsi)
            break;
        desc = desc->next;
    }
    return desc;
}

int get_interrupt_vector(uint32_t gsi, bool active_low, bool level_trig, int *cpu_id, uint32_t *vector)
{
    struct InterruptDescriptor *desc = find_descriptor(gsi);
    if (desc) {
        *cpu_id = desc->cpu;
        *vector = gsi;
        return 0;
    }

    if (cpus.size == 0) {
        fprintf(stderr, "Error: No CPUs available\n");
        return -ENOMEM;
    }

    // Select the CPU with the fewest interrupts allocated
    uint32_t cpu = 0;
    for (uint32_t i = 1; i < cpus.size; i++) {
        if (cpus.data[i].allocated_interrupts < cpus.data[cpu].allocated_interrupts)
            cpu = i;
    }

    struct InterruptDescriptor *new_desc = malloc(sizeof(*new_desc));
    if (!new_desc) {
        fprintf(stderr, "Error: Could not allocate memory for interrupt descriptor\n");
        return -ENOMEM;
    }

    new_desc->next = interrupts;
    new_desc->gsi = gsi;
    new_desc->cpu = cpu;
    interrupts = new_desc;

    cpus.data[cpu].allocated_interrupts++;

    *cpu_id = cpu;
    *vector = gsi;

    return 0;
}

int set_up_gsi(uint32_t gsi, bool active_low, bool level_trig, uint64_t task, pmos_port_t port, uint32_t *vector_out)
{
    int cpu_id = 0;
    uint32_t vector = 0;

    int result = get_interrupt_vector(gsi, active_low, level_trig, &cpu_id, &vector);
    if (result < 0) {
        fprintf(stderr, "Error: Could not get interrupt vector for GSI %u: %s\n", gsi, strerror(-result));
        return result;
    }

    result = register_interrupt(cpu_id, vector, task, port);
    if (result < 0) {
        fprintf(stderr, "Error: Could not register interrupt for GSI %u: %s\n", gsi, strerror(-result));
    }

    if (vector_out)
        *vector_out = vector;
    
    return result;
}

int install_isa_interrupt(uint32_t isa_pin, uint64_t task, pmos_port_t port, uint32_t *vector)
{
    return set_up_gsi(isa_pin, false, false, task, port, vector);
}