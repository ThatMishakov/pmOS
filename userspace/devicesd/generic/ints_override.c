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
#include <stdio.h>
#include <string.h>
#include <pmos/interrupts.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <uacpi/acpi.h>
#include <uacpi/tables.h>

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
    struct uacpi_table m;
    auto res = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &m);
    if (res != UACPI_STATUS_OK) {
        printf("Warning: Could not get MADT table\n");
        return;
    }

    struct acpi_madt *madt = (struct acpi_madt *)m.ptr;
    void *madt_end = (char *)(madt) + madt->hdr.length;

    struct acpi_entry_hdr *p = (struct acpi_entry_hdr *)((char *)(madt) + sizeof(struct acpi_madt));
    for (; (void *)(p) < madt_end; p = (struct acpi_entry_hdr *)((char *)(p) + p->length)) {
        struct acpi_entry_hdr *ee = (struct acpi_entry_hdr *)p;

        switch (ee->type) {
        case ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE: {
            struct acpi_madt_interrupt_source_override *e = (struct acpi_madt_interrupt_source_override *)p;
            // printf(" -> INT bus %x source %x int %x flags %x", e->bus, e->source,
            // e->global_system_interrupt, e->flags);

            uint8_t is_active_low      = (e->flags & ACPI_MADT_POLARITY_MASK) == ACPI_MADT_POLARITY_ACTIVE_LOW;
            uint8_t is_level_triggered = (e->flags & ACPI_MADT_TRIGGERING_MASK) == ACPI_MADT_TRIGGERING_LEVEL;
            register_redirect(e->source, e->gsi, is_active_low,
                              is_level_triggered);
        } break;
        default:
            break;
        }
    }

    uacpi_table_unref(&m);
}

right_request_t set_up_gsi(uint32_t gsi, bool active_low, bool level_trigger, pmos_port_t reply_port)
{
    pmos_right_t irq_right = 0;
    right_request_t result = {};

    uint32_t flags = 0;
    if (active_low)
        flags |= PMOS_INTERRUPT_ACTIVE_LOW;
    if (level_trigger)
        flags |= PMOS_INTERRUPT_LEVEL_TRIG;

    right_request_t irq_right_r = allocate_interrupt(gsi, flags);
    if (irq_right_r.result != 0) {
        fprintf(stderr, "Failed to allocate interrupt for GSI %u: %i (%s)\n", gsi, (int)irq_right_r.result,
               strerror(-irq_right_r.result));

        result.result = irq_right_r.result;
        goto end;
    }
    irq_right = irq_right_r.right;

    interrupt_info_t info = get_interrupt_affinity(irq_right);
    if (info.result != 0) {
        fprintf(stderr, "Failed to get interrupt affinity for GSI %u: %i (%s)\n", gsi, (int)info.result, strerror(-info.result));
        result.result = info.result;
        goto end;
    }

    auto set_result = set_affinity(TASK_ID_SELF, info.interrupt_affinity_cpu, 0);
    if (set_result != 0) {
        fprintf(stderr, "Failed to set affinity for GSI %u: %i (%s)\n", gsi, (int)set_result, strerror(-set_result));
        result.result = set_result;
        goto end;
    }

    result = set_interrupt(irq_right, reply_port);
    if (result.result != 0) {
        fprintf(stderr, "Failed to register interrupt for GSI %u: %i (%s)\n", gsi, (int)result.result, strerror(-result.result));
        // goto end;
    }
end:
    delete_right(irq_right);
    return result;
}

right_request_t install_isa_interrupt(uint32_t isa_pin, pmos_port_t port)
{
    int_redirect_descriptor desc = get_for_int(isa_pin);
    return set_up_gsi(desc.destination, desc.active_low, desc.level_trig, port);
}