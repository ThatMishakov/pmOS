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

#include <ioapic/ints_override.h>
#include <stddef.h>
#include <stdlib.h>
#include <acpi/acpi.h>
#include <stdio.h>
#include <string.h>

typedef struct redir_list {
    struct redir_list *next;
    int_redirect_descriptor desc;
} redir_list;

redir_list *redir_list_head = NULL;

void register_redirect(uint32_t source, uint32_t to, uint8_t active_low, uint8_t level_trig)
{
    redir_list *node = malloc(sizeof(redir_list));

    node->desc.source      = source;
    node->desc.destination = to;
    node->desc.active_low  = active_low;
    node->desc.level_trig  = level_trig;

    node->next      = redir_list_head;
    redir_list_head = node;
}

int_redirect_descriptor get_for_int(uint32_t intno)
{
    redir_list *p = redir_list_head;

    while (p && p->desc.source != intno)
        p = p->next;

    if (p) {
        return p->desc;
    } else {
        int_redirect_descriptor desc = {intno, intno, 0, 0};
        return desc;
    }
}

int_redirect_descriptor isa_gsi_mapping(uint32_t intno)
{
    return get_for_int(intno);
}

void init_int_redirects()
{
    MADT *madt = (MADT *)get_table("APIC", 0);

    if (madt == NULL) {
        printf("Warning: Did not find MADT table\n");
        return;
    }

    void *madt_end = (char *)(madt) + madt->header.length;

    MADT_entry *p = madt->entries;
    for (; (void *)(p) < madt_end; p = (MADT_entry *)((char *)(p) + p->length)) {
        // printf("MADT entry type %x size %i", p->type, p->length);

        switch (p->type) {
        case MADT_LOCAL_APIC_NMI_TYPE: {
            // MADT_LAPIC_NMI_entry* e = (MADT_LAPIC_NMI_entry*)p;
            // printf(" -> LAPIC NMI CPU ID: %x flags %X INT%i\n", e->ACPI_CPU_UID, e->Flags,
            // e->LINT_ID);
        } break;
        case MADT_INT_OVERRIDE_TYPE: {
            MADT_INT_entry *e = (MADT_INT_entry *)p;
            // printf(" -> INT bus %x source %x int %x flags %x", e->bus, e->source,
            // e->global_system_interrupt, e->flags);

            uint8_t is_active_low      = (e->flags & 0x03) == 0b11;
            uint8_t is_level_triggered = ((e->flags >> 2) & 0x03) == 0b11;
            register_redirect(e->source, e->global_system_interrupt, is_active_low,
                              is_level_triggered);
        } break;
        default:
            break;
        }
    }
}

int set_up_gsi(uint32_t gsi, bool active_low, bool level_trig, uint64_t task, pmos_port_t port, uint32_t *vector_out)
{
    unsigned cpu_id = 0;
    unsigned vector = 0;

    unsigned flags = (active_low ? PMOS_INTERRUPT_ACTIVE_LOW : 0) | (level_trig ? PMOS_INTERRUPT_LEVEL_TRIG : 0);
    auto vec_result = allocate_interrupt(gsi, flags);
    if (vec_result.result < 0) {
        fprintf(stderr, "Error: Could not get interrupt vector for GSI %u: %s\n", gsi, strerror(-vec_result.result));
        return vec_result.result;
    }

    cpu_id = vec_result.cpu;
    vector = vec_result.vector;

    printf("Got vector %u for GSI %u\n", vector, gsi);

    auto result = register_interrupt(cpu_id - 1, vector, task, port);
    if (result < 0) {
        fprintf(stderr, "Error: Could not register interrupt for GSI %u: %s\n", gsi, strerror(-result));
    }

    if (vector_out)
        *vector_out = vector;
    
    return result;
}

int install_isa_interrupt(uint32_t isa_pin, uint64_t task, pmos_port_t port, uint32_t *vector)
{
    int_redirect_descriptor desc = get_for_int(isa_pin);
    return set_up_gsi(desc.destination, desc.active_low, desc.level_trig, task, port, vector);
}