/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IOAPIC_H
#define IOAPIC_H
#include <configuration.h>
#include <stdbool.h>
#include <stdint.h>

struct IOAPICID_struct {
    uint32_t reserved0 : 24;
    uint8_t id : 8;
    //uint8_t reserved1 : 4;
} __attribute__((packed));

typedef union {
    struct IOAPICID_struct bits;
    uint32_t asint;
} IOAPICID;

struct __attribute__((packed)) IOAPICVER_struct {
    uint8_t apic_version;
    uint8_t reserved0;
    uint8_t max_redir_entry;
    uint8_t reserved1;
};

typedef union {
    uint32_t asint;
    struct IOAPICVER_struct bits;
} IOAPICVER;

struct __attribute__((packed)) IOREDTBL_struct {
    uint8_t int_vector : 8;
    uint8_t DELMOD : 3;
    uint8_t DESTMOD : 1;
    uint8_t DELIVS : 1;
    uint8_t INTPOL : 1;
    uint8_t REM_IRR : 1;
    uint8_t TRIGMOD : 1;
    uint8_t mask : 1;
    uint16_t reserved : 15;
    uint32_t destination;
};

typedef union {
    uint32_t asints[2];
    uint64_t aslong;
    struct IOREDTBL_struct bits;
} IOREDTBL;

typedef struct ioapic_descriptor {
    volatile uint32_t *virt_addr;
    uint64_t phys_addr;
    uint32_t int_base;
    uint32_t max_int;
    uint64_t *redirection_entries_mirrored;
} ioapic_descriptor;

typedef struct ioapic_list {
    ioapic_descriptor desc;
    struct ioapic_list *next;
} ioapic_list;

void init_ioapic();

// Returns pointer to IOAPIC virtual (mapped) address o NULL if not found
ioapic_descriptor *get_ioapic_for_int(uint32_t intno);

// Returns first IOAPIC (for ints 0-23) or NULL on error
ioapic_descriptor *get_first_ioapic();

uint32_t ioapic_read_reg(volatile uint32_t *ioapic, uint32_t reg_sel);
IOAPICID ioapic_read_ioapicid(volatile uint32_t *ioapic);
IOAPICVER ioapic_read_ioapicver(volatile uint32_t *ioapic);

IOREDTBL ioapic_read_redir_reg(volatile uint32_t *ioapic, int index);
void ioapic_write_redir_reg(volatile uint32_t *ioapic, int index, IOREDTBL entry);

void ioapic_write_reg(volatile uint32_t *ioapic, uint32_t reg_sel, uint32_t value);
void ioapic_set_redir_reg(volatile uint32_t *ioapic, int index, IOREDTBL value);

bool program_ioapic(uint8_t cpu_int_vector, uint32_t ext_int_vector);
bool program_ioapic_manual(uint8_t cpu_int_vector, uint32_t ext_int_vector, bool active_low,
                           bool level_trig);

void ioapic_mask_int(struct ioapic_descriptor *ioapic, uint32_t intno);

uint8_t ioapic_get_int(struct int_task_descriptor desc, uint8_t line, bool active_low,
                       bool level_trig, bool check_free);

#endif