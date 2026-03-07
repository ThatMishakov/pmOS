#include <uacpi/tables.h>
#include <uacpi/acpi.h>
#include <kern_logger/kern_logger.hh>


using namespace kernel;

void init_hpet()
{
    struct uacpi_table table;
    auto ret = uacpi_table_find_by_signature(ACPI_HPET_SIGNATURE, &table);
    if (uacpi_unlikely_error(ret)) {
        log::serial_logger.printf("Couldn't get HPET table: %s", uacpi_status_to_string(ret));
        return;
    }

    acpi_hpet *hpet = (acpi_hpet *)table.ptr;

    u64 address = hpet->address.address;
    u64 period  = hpet->min_clock_tick;
    u64 number = hpet->number;

    uacpi_table_unref(&table);

    log::serial_logger.printf("HPET address: %p\n", address);
}