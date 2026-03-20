#include <uacpi/tables.h>
#include <uacpi/acpi.h>
#include <kern_logger/kern_logger.hh>


using namespace kernel;

namespace kernel::x86::hpet {

struct HPET {
    u64 phys_addr;
    u16 clock_tick;
    unsigned number;
    
    void *virt_addr;
};

HPET *hpet = nullptr;

void init_hpet()
{
    struct uacpi_table table;
    auto ret = uacpi_table_find_by_signature(ACPI_HPET_SIGNATURE, &table);
    if (uacpi_unlikely_error(ret)) {
        log::serial_logger.printf("Couldn't get HPET table: %s", uacpi_status_to_string(ret));
        return;
    }

    acpi_hpet *hpet_table = (acpi_hpet *)table.ptr;

    u64 address = hpet_table->address.address;
    auto period  = hpet_table->min_clock_tick;
    unsigned number = hpet_table->number;

    uacpi_table_unref(&table);

    log::serial_logger.printf("Found HPET! Address 0x%lx, period %i, number %i\n", address, period, number);
    log::global_logger.printf("Found HPET! Address 0x%lx, period %i, number %i\n", address, period, number);

    hpet = new HPET();
    if (!hpet)
        panic("Failed to allocate memory for HPET!\n");

    hpet->phys_addr = address;
    hpet->clock_tick = period;
    hpet->number = number;
}

}