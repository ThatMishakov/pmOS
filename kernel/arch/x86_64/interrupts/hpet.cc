#include <acpi/acpi.h>
#include <acpi/acpi.hh>
#include <kern_logger/kern_logger.hh>

using namespace kernel;

using HPET = HPET_Description_Table;

HPET *get_hpet()
{
    static HPET *hpet_virt = nullptr;
    static bool have_acpi   = true;

    if (hpet_virt == nullptr and have_acpi) {
        u64 hpet_phys = get_table(0x54455048); // HPET
        if (hpet_phys == 0) {
            have_acpi = false;
            return nullptr;
        }

        ACPISDTHeader h;
        copy_from_phys(hpet_phys, &h, sizeof(h));

        hpet_virt = (HPET *)malloc(h.length);
        if (hpet_virt == nullptr) {
            log::serial_logger.printf("Could not allocate memory for HPET\n");
            return nullptr;
        }
        copy_from_phys(hpet_phys, hpet_virt, h.length);
    }

    return hpet_virt;
}

void init_hpet()
{
    HPET *hpet = get_hpet();
    if (hpet == nullptr) {
        log::serial_logger.printf("Could not get HPET\n");
        return;
    }

    u64 address = hpet->BASE_ADDRESS.Address;
    u64 period  = hpet->Min_Clock_tick;
    u64 number = hpet->HPET_Number;

    log::serial_logger.printf("HPET address: %p\n", address);
}