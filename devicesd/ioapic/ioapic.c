#include <ioapic/ioapic.h>
#include <acpi/acpi.h>
#include <stddef.h>
#include <stdio.h>

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
        }
            break;
        case MADT_IOAPIC_entry_type: {
            MADT_IOAPIC_entry* e = (MADT_IOAPIC_entry*)p;
            printf(" -> IOAPIC id %x addr 0x%X base %x", e->ioapic_id, e->ioapic_addr, e->global_system_interrupt_base);
        }
            break;
        default:
            break;
        }

        printf("\n");
    }
}