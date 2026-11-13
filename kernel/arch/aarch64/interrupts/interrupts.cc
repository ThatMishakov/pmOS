#include "interrupts.hh"
#include <utils.hh>
#include <uacpi/tables.h>
#include <uacpi/acpi.h>
#include "gicv3.hh"
#include <kern_logger/kern_logger.hh>
#include <pmos/utility/scope_guard.hh>


using namespace kernel::arm::interrupts;
using namespace kernel;

namespace {

acpi_entry_hdr *get_madt_entry(acpi_madt *madt, u8 type, int idx)
{
    size_t offset = sizeof(acpi_madt);
    int iidx = 0;
    while (offset < madt->hdr.length) {
        acpi_entry_hdr *ee = (acpi_entry_hdr *)((char *)madt + offset);
        offset += ee->length;
        if (ee->type != type)
            continue;

        if (iidx++ == idx)
            return ee;
    }

    return nullptr;
}

bool init_interrupts_acpi()
{
    uacpi_table m;
    auto res = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &m);
    if (res != UACPI_STATUS_OK) {
        log::serial_logger.printf("Couldn't get MADT table...\n");
        return false;
    }
    auto guard = pmos::utility::make_scope_guard([&]{
        uacpi_table_unref(&m);
    });

    acpi_madt *madt = (acpi_madt *)m.ptr;

    auto e = (acpi_madt_gicd *)get_madt_entry(madt, ACPI_MADT_ENTRY_TYPE_GICD, 0);
    if (!e) {
        log::serial_logger.printf("Couldn't get GICD entry from MADT table...\n");
        return false;
    }

    switch (e->gic_version) {
    case 0x01 ... 0x02:
        panic("System has GICv1 or GICv2 which is not yet supported\n");
        break;
    case 0x03 ... 0x04:
        init_gicv3();
        break;
    default:
        panic("Unsupported GIC version %u\n", e->gic_version);
        break;
    }

    return true;
}


}

void kernel::arm::interrupts::init_interrupt_controllers()
{
    bool success = init_interrupts_acpi();
    if (success)
        return;

    // TODO: DTs

    panic("Could not initialize interrupt controllers with ACPI, DTs not implemented\n");
}
