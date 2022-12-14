#ifndef IOAPIC_H
#define IOAPIC_H
#include <stdint.h>

typedef union {
    struct {
        uint32_t reserved0: 24;
        uint8_t id: 4;
        uint8_t reserved1: 4;
    } bits;
    uint32_t asint;
} IOAPICID;

typedef union {
    struct {
        uint8_t apic_version;
        uint8_t reserved0;
        uint8_t max_redir_entry;
        uint8_t reserved1;
    } bits;
    uint32_t asint;
} IOAPICVER;

typedef union {
    struct {
        uint8_t int_vector;
        uint8_t DELMOD : 3;
        uint8_t DESTMOD: 1;
        uint8_t DELIVS : 1;
        uint8_t INTPOL : 1;
        uint8_t REM_IRR: 1;
        uint8_t TRIGMOD: 1;
        uint8_t mask   : 1;
        uint64_t reserved: 39;
        uint8_t destination;
    } bits;
    uint32_t asints[2];
} IOREDTBL;

typedef struct ioapic_descriptor {
    uint32_t* virt_addr;
    uint64_t phys_addr;
    uint32_t int_base;
    uint32_t max_int;
} ioapic_descriptor;

typedef struct ioapic_list
{
    ioapic_descriptor desc;
    struct ioapic_list* next;
} ioapic_list;

void init_ioapic();

uint32_t ioapic_read_reg(uint32_t* ioapic, uint32_t reg_sel);
IOAPICID ioapic_read_ioapicid(uint32_t* ioapic);
IOAPICVER ioapic_read_ioapicver(uint32_t* ioapic);

IOREDTBL ioapic_read_redir_reg(uint32_t* ioapic, int index);

void ioapic_write_reg(uint32_t* ioapic, uint32_t reg_sel, uint32_t value);
void ioapic_set_redir_reg(uint32_t* ioapic, int index, IOREDTBL value);

#endif