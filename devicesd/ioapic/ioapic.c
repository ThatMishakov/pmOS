#include <ioapic/ioapic.h>
#include <acpi/acpi.h>
#include <stddef.h>
#include <stdio.h>
#include <phys_map/phys_map.h>
#include <ioapic/ints_override.h>
#include <stdbool.h>

ioapic_list* ioapic_list_root = NULL;

uint32_t ioapic_read_reg(volatile uint32_t* ioapic, uint32_t reg_sel)
{
    ioapic[0] = reg_sel;
    return ioapic[0x10/4];
}

void ioapic_write_reg(volatile uint32_t* ioapic, uint32_t reg_sel, uint32_t value)
{
    ioapic[0] = reg_sel;
    ioapic[4] = value;
}

IOAPICID ioapic_read_ioapicid(volatile uint32_t* ioapic)
{
    IOAPICID i;
    i.asint = ioapic_read_reg(ioapic, 0);
    return i;
}

IOAPICVER ioapic_read_ioapicver(volatile uint32_t* ioapic)
{
    IOAPICVER i;
    i.asint = ioapic_read_reg(ioapic, 1);
    return i;
}

void init_ioapic_at(uint16_t id, uint64_t address, uint32_t base)
{
    uint32_t* ioapic = map_phys((void*)address, 0x20);

    IOAPICID i = ioapic_read_ioapicid(ioapic);
    if (i.bits.id != id) {
        printf("Warning: IOAPIC id does not match!\n");
        return;
    }

    ioapic_list* node = malloc(sizeof(ioapic_list));
    node->next = NULL;
    node->desc.phys_addr = address;
    node->desc.virt_addr = ioapic;
    node->desc.int_base = base;

    IOAPICVER v = ioapic_read_ioapicver(ioapic);
    node->desc.max_int = base + v.bits.max_redir_entry;

    if (ioapic_list_root == NULL) {
        ioapic_list_root = node;
    } else {
        ioapic_list* c = ioapic_list_root;
        while (c->next != NULL) {
            c = c->next;
        }

        c->next = node;
    }
}

ioapic_descriptor* get_ioapic_for_int(uint32_t intno)
{
    ioapic_list* node = ioapic_list_root;

    while (node != NULL) {
        if (node->desc.int_base <= intno && node->desc.max_int >= intno)
            return &node->desc;

        node = node->next;
    } 

    return NULL;
}

void init_ioapic()
{
    MADT* madt = (MADT*)get_table("APIC", 0);

    if (madt == NULL) {
        printf("Warning: Did not find MADT table\n");
        return;
    }

    void* madt_end = (char*)(madt) + madt->header.length;

    MADT_entry *p = madt->entries;
    for (; (void*)(p) < madt_end; p = (MADT_entry*)((char*)(p) + p->length)) {
        printf("MADT entry type %x size %i", p->type, p->length);

        switch (p->type) {
        case MADT_LOCAL_APIC_NMI_TYPE: {
            MADT_LAPIC_NMI_entry* e = (MADT_LAPIC_NMI_entry*)p;
            printf(" -> LAPIC NMI CPU ID: %x flags %X INT%i\n", e->ACPI_CPU_UID, e->Flags, e->LINT_ID);
        }
            break;
        case MADT_INT_OVERRIDE_TYPE: {
            MADT_INT_entry* e = (MADT_INT_entry*)p;
            printf(" -> INT bus %x source %x int %x flags %x", e->bus, e->source, e->global_system_interrupt, e->flags);

            uint8_t is_active_low = (e->flags & 0x03) == 0b11;
            uint8_t is_level_triggered = ((e->flags >> 2) & 0x03) == 0b11;
            register_redirect(e->source, e->global_system_interrupt, is_active_low, is_level_triggered);
        }
            break;
        case MADT_IOAPIC_entry_type: {
            MADT_IOAPIC_entry* e = (MADT_IOAPIC_entry*)p;
            printf(" -> IOAPIC id %x addr 0x%X base %x", e->ioapic_id, e->ioapic_addr, e->global_system_interrupt_base);
            init_ioapic_at(e->ioapic_id, e->ioapic_addr, e->global_system_interrupt_base);
        }
            break;
        default:
            break;
        }

        printf("\n");
    }
}

IOREDTBL ioapic_read_redir_reg(volatile uint32_t* ioapic, int index)
{
    int i = 0x10 + index*2;

    IOREDTBL v;
    v.asints[0] = ioapic_read_reg(ioapic, i);
    v.asints[1] = ioapic_read_reg(ioapic, i+1);

    return v;
}

void ioapic_write_redir_reg(volatile uint32_t* ioapic, int index, IOREDTBL entry)
{
    int i = 0x10 + index*2;
    ioapic_write_reg(ioapic, i,   entry.asints[0]);
    ioapic_write_reg(ioapic, i+2, entry.asints[1]);
}

void ioapic_set_redir_reg(volatile uint32_t* ioapic, int index, IOREDTBL value)
{
    int i = 0x10 + index*2;

    ioapic_write_reg(ioapic, i,   value.asints[0]);
    ioapic_write_reg(ioapic, i+1, value.asints[1]);
}

bool program_ioapic(uint8_t cpu_int_vector, uint32_t ext_int_vector)
{
    int_redirect_descriptor desc = get_for_int(ext_int_vector);

    ioapic_descriptor* ioapic_desc = get_ioapic_for_int(desc.destination);
    if (ioapic_desc == NULL) return false;

    uint32_t* ioapic = ioapic_desc->virt_addr;
    uint32_t ioapic_base = ext_int_vector - ioapic_desc->int_base;

    IOREDTBL i = ioapic_read_redir_reg(ioapic, ioapic_base);
    i.bits.int_vector = cpu_int_vector;
    i.bits.DELMOD = 0b000;
    i.bits.DESTMOD = 0;
    i.bits.INTPOL = desc.active_low;
    i.bits.TRIGMOD = desc.level_trig;
    i.bits.mask = 0;
    i.bits.destination = 0x00;

    ioapic_write_redir_reg(ioapic, ioapic_base, i);

    return true;
}