#include <uacpi/uacpi.h>
#include <uacpi/kernel_api.h>
#include <types.hh>
#include <kern_logger/kern_logger.hh>

using namespace kernel::log;

constexpr phys_addr_t RSDP_INITIALIZER = -1ULL;
phys_addr_t rsdp                       = -1ULL;

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address)
{
    if (rsdp == RSDP_INITIALIZER)
        return UACPI_STATUS_NOT_FOUND;

    *out_rsdp_address = rsdp;
    return UACPI_STATUS_OK;
}

size_t uacpi_early_size = 0x1000;
char *uacpi_temp_buffer = nullptr;

void init_acpi(phys_addr_t rsdp_addr)
{
    if (rsdp_addr == 0) {
        serial_logger.printf("Warning: ACPI RSDP addr is 0...");
        return;
    }

    rsdp = rsdp_addr;

    for (;;) {
        uacpi_temp_buffer = new char[uacpi_early_size];
        if (!uacpi_temp_buffer)
            panic("Couldn't allocate memory for uACPI early buffer");
    
        auto ret = uacpi_setup_early_table_access((void *)uacpi_temp_buffer, uacpi_early_size);
        if (ret == UACPI_STATUS_OK)
        break;

        delete uacpi_temp_buffer;
        if (ret == UACPI_STATUS_OUT_OF_MEMORY) {
            uacpi_early_size += 4096;
        } else {
            serial_logger.printf("uacpi_initialize error: %s", uacpi_status_to_string(ret));
            return;
        }
    }
}