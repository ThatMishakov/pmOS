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

#include <acpi/acpi.h>
#include <ioapic/ints_override.h>
#include <ioapic/ioapic.h>
#include <phys_map/phys_map.h>
#include <pmos/system.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <uacpi/acpi.h>
#include <errno.h>
#include <pthread.h>
#include <interrupts.h>
#include <string.h>

ioapic_list *ioapic_list_root = NULL;

uint32_t ioapic_read_reg(volatile uint32_t *ioapic, uint32_t reg_sel)
{
    ioapic[0] = reg_sel;
    return ioapic[0x10 / 4];
}

void ioapic_write_reg(volatile uint32_t *ioapic, uint32_t reg_sel, uint32_t value)
{
    ioapic[0] = reg_sel;
    ioapic[4] = value;
}

IOAPICID ioapic_read_ioapicid(volatile uint32_t *ioapic)
{
    IOAPICID i;
    i.asint = ioapic_read_reg(ioapic, 0);
    return i;
}

IOAPICVER ioapic_read_ioapicver(volatile uint32_t *ioapic)
{
    IOAPICVER i;
    i.asint = ioapic_read_reg(ioapic, 1);
    return i;
}

void init_ioapic_at(uint16_t id, uint64_t address, uint32_t base)
{
    uint32_t *ioapic = map_phys((void *)address, 0x20);

    IOAPICID i = ioapic_read_ioapicid(ioapic);
    if (i.bits.id != id) {
        printf("Warning: IOAPIC id does not match! (expected 0x%x got 0x%x)\n", id, i.bits.id);
    }

    ioapic_list *node    = malloc(sizeof(ioapic_list));
    node->next           = NULL;
    node->desc.phys_addr = address;
    node->desc.virt_addr = ioapic;
    node->desc.int_base  = base;

    IOAPICVER v        = ioapic_read_ioapicver(ioapic);
    node->desc.max_int = base + v.bits.max_redir_entry;

    if (ioapic_list_root == NULL) {
        ioapic_list_root = node;
    } else {
        ioapic_list *c = ioapic_list_root;
        while (c->next != NULL) {
            c = c->next;
        }

        c->next = node;
    }

    for (unsigned int i = 0; i < node->desc.max_int; ++i)
        ioapic_mask_int(ioapic, i);

    printf("IOAPIC %i at %#lX base %i max %i\n", id, address, base, node->desc.max_int);
}

ioapic_descriptor *get_ioapic_for_int(uint32_t intno)
{
    ioapic_list *node = ioapic_list_root;

    while (node != NULL) {
        if (node->desc.int_base <= intno && node->desc.max_int >= intno)
            return &node->desc;

        node = node->next;
    }

    return NULL;
}

ioapic_descriptor *get_first_ioapic() { return get_ioapic_for_int(0); }

void init_cpus();

void init_ioapic()
{
    init_cpus();
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
        case MADT_IOAPIC_entry_type: {
            MADT_IOAPIC_entry *e = (MADT_IOAPIC_entry *)p;
            // printf(" -> IOAPIC id %x addr 0x%X base %x", e->ioapic_id, e->ioapic_addr,
            // e->global_system_interrupt_base);
            init_ioapic_at(e->ioapic_id, e->ioapic_addr, e->global_system_interrupt_base);
        } break;
        default:
            break;
        }

        // printf("\n");
    }
}

IOREDTBL ioapic_read_redir_reg(volatile uint32_t *ioapic, int index)
{
    int i = 0x10 + index * 2;

    IOREDTBL v;
    v.asints[0] = ioapic_read_reg(ioapic, i);
    v.asints[1] = ioapic_read_reg(ioapic, i + 1);

    return v;
}

void ioapic_write_redir_reg(volatile uint32_t *ioapic, int index, IOREDTBL entry)
{
    int i = 0x10 + index * 2;
    ioapic_write_reg(ioapic, i, entry.asints[0]);
    ioapic_write_reg(ioapic, i + 1, entry.asints[1]);
}

void ioapic_set_redir_reg(volatile uint32_t *ioapic, int index, IOREDTBL value)
{
    int i = 0x10 + index * 2;

    ioapic_write_reg(ioapic, i, value.asints[0]);
    ioapic_write_reg(ioapic, i + 1, value.asints[1]);
}

bool program_ioapic(uint8_t cpu_int_vector, uint32_t ext_int_vector)
{
    int_redirect_descriptor desc = get_for_int(ext_int_vector);

    ioapic_descriptor *ioapic_desc = get_ioapic_for_int(desc.destination);
    if (ioapic_desc == NULL)
        return false;

    volatile uint32_t *ioapic = ioapic_desc->virt_addr;
    uint32_t ioapic_base      = ext_int_vector - ioapic_desc->int_base;

    IOREDTBL i         = ioapic_read_redir_reg(ioapic, ioapic_base);
    i.bits.int_vector  = cpu_int_vector;
    i.bits.DELMOD      = 0b000;
    i.bits.DESTMOD     = 0;
    i.bits.INTPOL      = desc.active_low;
    i.bits.TRIGMOD     = desc.level_trig;
    i.bits.mask        = 0;
    i.bits.destination = get_lapic_id(0).value;

    ioapic_write_redir_reg(ioapic, ioapic_base, i);

    return true;
}

bool program_ioapic_manual(uint8_t cpu_int_vector, uint32_t ext_int_vector, bool active_low,
                           bool level_trig)
{
    ioapic_descriptor *ioapic_desc = get_ioapic_for_int(ext_int_vector);
    if (ioapic_desc == NULL)
        return false;

    volatile uint32_t *ioapic = ioapic_desc->virt_addr;
    uint32_t ioapic_base      = ext_int_vector - ioapic_desc->int_base;

    IOREDTBL i         = ioapic_read_redir_reg(ioapic, ioapic_base);
    i.bits.int_vector  = cpu_int_vector;
    i.bits.DELMOD      = 0b000;
    i.bits.DESTMOD     = 0;
    i.bits.INTPOL      = active_low;
    i.bits.TRIGMOD     = level_trig;
    i.bits.mask        = 0;
    i.bits.destination = get_lapic_id(0).value;

    ioapic_write_redir_reg(ioapic, ioapic_base, i);

    return true;
}

void ioapic_mask_int(volatile uint32_t *ioapic, uint32_t intno)
{
    IOREDTBL i = ioapic_read_redir_reg(ioapic, intno);

    i.bits.mask = 1;

    ioapic_write_redir_reg(ioapic, intno, i);
}

struct InterruptMapping {
    struct InterruptMapping *next;
    uint32_t gsi;

    int cpu_id;
    uint8_t vector;
};

struct InterruptMapping *mappings = NULL;

struct InterruptMapping *get_mapping(uint32_t gsi)
{
    struct InterruptMapping *m = mappings;
    while (m)
    {
        if (m->gsi == gsi)
            break;
        ++m;
    }
    return m;
}

void push_mapping(struct InterruptMapping *m)
{
    m->next = mappings;
    mappings = m;
}

int assign_int_vector(int *cpu_id_out, uint8_t *vector_out);
uint32_t lapic_id_for_cpu(int cpu_id);

pthread_mutex_t int_vector_mutex = PTHREAD_MUTEX_INITIALIZER;

int get_interrupt_vector(uint32_t gsi, bool active_low, bool level_trig, int *cpu_id_out, uint8_t *vector_out)
{
    pthread_mutex_lock(&int_vector_mutex);
    struct InterruptMapping *m = get_mapping(gsi);
    if (m) {
        *cpu_id_out = m->cpu_id;
        *vector_out = m->vector;
        pthread_mutex_unlock(&int_vector_mutex);
        return 0;
    }

    ioapic_descriptor *ioapic_desc = get_ioapic_for_int(gsi);
    if (!ioapic_desc) {
        fprintf(stderr, "Error: Could not find IOAPIC for interrupt %u\n", gsi);
        pthread_mutex_unlock(&int_vector_mutex);
        return -EINVAL;
    }

    struct InterruptMapping *new_mapping = malloc(sizeof(*new_mapping));
    if (!new_mapping) {
        printf("Error: Could not allocate memory for new interrupt mapping\n");
        pthread_mutex_unlock(&int_vector_mutex);
        return -ENOMEM;
    }

    int result = assign_int_vector(cpu_id_out, vector_out);
    if (result < 0) {
        free(new_mapping);
        pthread_mutex_unlock(&int_vector_mutex);
        return result;
    }

    new_mapping->gsi = gsi;
    new_mapping->cpu_id = *cpu_id_out;
    new_mapping->vector = *vector_out;
    push_mapping(new_mapping);


    // Program IOAPIC
    volatile uint32_t *ioapic = ioapic_desc->virt_addr;
    uint32_t ioapic_base      = gsi - ioapic_desc->int_base;

    IOREDTBL i         = ioapic_read_redir_reg(ioapic, ioapic_base);
    i.bits.int_vector  = *vector_out;
    i.bits.DELMOD      = 0b000;
    i.bits.DESTMOD     = 0;
    i.bits.INTPOL      = active_low;
    i.bits.TRIGMOD     = level_trig;
    i.bits.mask        = 0;
    i.bits.destination = lapic_id_for_cpu(*cpu_id_out);

    ioapic_write_redir_reg(ioapic, ioapic_base, i);

    pthread_mutex_unlock(&int_vector_mutex);

    printf("New interrupt mapping: GSI %u -> CPU %d, vector %u\n", gsi, *cpu_id_out, *vector_out);

    return 0;
}

int set_up_gsi(uint32_t gsi, bool active_low, bool level_trig, uint64_t task, pmos_port_t port, uint32_t *vector_out)
{
    int cpu_id = 0;
    uint8_t vector = 0;

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
    int_redirect_descriptor desc = get_for_int(isa_pin);
    return set_up_gsi(desc.destination, desc.active_low, desc.level_trig, task, port, vector);
}

void configure_interrupts_for(Message_Descriptor *desc, IPC_Reg_Int *m)
{
    uint32_t gsi = 0;
    bool active_low = false;
    bool level_trig = false;
    int result = 0;

    uint32_t vector = 0;

    if (m->flags & IPC_Reg_Int_FLAG_EXT_INTS) {
        int_redirect_descriptor desc = get_for_int(m->intno);
        gsi = desc.destination;
        active_low = desc.active_low;
        level_trig = desc.level_trig;
    } else {
        result = -ENOSYS;
        goto finish;
    }

    if (m->dest_task == 0) {
        result = -EINVAL;
        goto finish;
    }

    result = set_up_gsi(gsi, active_low, level_trig, m->dest_task, m->reply_chan, &vector);
finish:
    IPC_Reg_Int_Reply reply;
    reply.type = IPC_Reg_Int_Reply_NUM;
    reply.status = result;
    reply.intno = vector;
    result = send_message_port(m->reply_chan, sizeof(reply), (char*)&reply);
    if (result < 0)
        fprintf(stderr, "Warning could not reply to task %#lx port %#lx in configure_interrupts_for: %i (%s)\n", desc->sender, m->reply_chan, result, strerror(-result));
}