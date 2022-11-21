#include <acpi.h>
#include <multiboot2.h>
#include <io.h>
#include <syscall.h>

RSDP_descriptor* rsdp_desc = 0;
RSDP_descriptor20* rsdp20_desc = 0;

char doChecksum(ACPISDTHeader *tableHeader)
{
    unsigned char sum = 0;
 
    for (unsigned int i = 0; i < tableHeader->length; i++)
    {
        sum += ((char *) tableHeader)[i];
    }
 
    return sum == 0;
}

char RDSP_checksum(RSDP_descriptor *desc)
{
    unsigned char sum = 0;
    for (unsigned int i = 0; i < sizeof(RSDP_descriptor); ++i)
        sum += ((char *)desc)[i];

    return sum == 0;
}

char RDSP_checksum_20(RSDP_descriptor20 *desc)
{
    unsigned char sum = 0;
    for (unsigned int i = 0; i < desc->length; ++i)
        sum += ((char *)desc)[i];

    return sum == 0;
}

void printACPI(void *RootSDT)
{
    XSDT *xsdt = (XSDT *) RootSDT;
    int entries = (xsdt->h.length - sizeof(xsdt->h)) / 8;
 
    for (int i = 0; i < entries; i++)
    {
        ACPISDTHeader *h = (ACPISDTHeader *) xsdt->PointerToOtherSDT[i];
        map_phys((u64)(h)&~0xfffULL, (u64)(h)&~0xfffULL, 1, 0x3);
        print_str("ACPI ");
        print_hex(h);
        print_str(" signature ");
        syscall(SYSCALL_SEND_MSG_PORT, 1, 4, h->signature);
        print_str("\n");
    }
}

MADT* getMADT()
{
    static const uint32_t apic_signature = 0x43495041;
    if (rsdp20_desc != 0) {
        XSDT *xsdt = rsdp20_desc->xsdt_address;
        uint32_t entries = (xsdt->h.length - sizeof(xsdt->h)) / 8;

        for (uint32_t i = 0; i < entries; ++i) {
            ACPISDTHeader *h = (ACPISDTHeader *) xsdt->PointerToOtherSDT[i];
            map_phys((u64)(h)&~0xfffULL, (u64)(h)&~0xfffULL, 1, 0x3);
            if (*(uint32_t*)(h->signature) == apic_signature) return (MADT*)h;
        }
    } if (rsdp_desc != 0) {
        RSDT *rsdt = rsdp_desc->rsdt_address;
        uint32_t entries = (rsdt->h.length - sizeof(rsdt->h)) / 4;

        for (uint32_t i = 0; i < entries; ++i) {
            ACPISDTHeader *h = (ACPISDTHeader *) rsdt->PointerToOtherSDT[i];
            map_phys((u64)(h)&~0xfffULL, (u64)(h)&~0xfffULL, 1, 0x3);
            if (*(uint32_t*)(h->signature) == apic_signature) return (MADT*)h;
        }
    }
    return 0;
}

void (*kernel_cpu_init)(void);

void init_acpi(unsigned long multiboot_info_str)
{
    struct multiboot_tag_module * mod = 0;

    for (struct multiboot_tag * tag = (struct multiboot_tag *) (multiboot_info_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
          tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
            if (tag->type == MULTIBOOT_TAG_TYPE_ACPI_OLD) {
                struct multiboot_tag_old_acpi *acpi_tag = (struct multiboot_tag_old_acpi*)tag;
                rsdp_desc = (RSDP_descriptor*)&acpi_tag->rsdp;
            } else if (tag->type == MULTIBOOT_TAG_TYPE_ACPI_NEW) {
                struct multiboot_tag_new_acpi *acpi_tag = (struct multiboot_tag_old_acpi*)tag;
                rsdp20_desc = (RSDP_descriptor20*)acpi_tag->rsdp;
            }
    }

    if (rsdp20_desc != 0) {
        //print_str("Found new ACPI str\n");
        //print_hex(rsdp20_desc->xsdt_address);
        //print_str(" | ");
        //print_hex(RDSP_checksum_20(rsdp20_desc));
        //print_str("\n");
        uint64_t xsdt_addr = rsdp20_desc->xsdt_address;
        map_phys(xsdt_addr&~0xfffULL, xsdt_addr&~0xfffULL, 1, 0x3);
        //printACPI(xsdt_addr);

    } else if (rsdp_desc != 0) {
        //print_str("Found ACPI str\n");
        //print_hex(rsdp_desc->rsdt_address);
        //print_str(" | ");
        //print_hex(RDSP_checksum(rsdp_desc));
        //print_str("\n");
        uint32_t rsdt_addr = rsdp_desc->rsdt_address;
        map_phys(rsdt_addr&~0xfffULL, rsdt_addr&~0xfffULL, 1, 0x3);
    } else {
        print_str("!!! Did not find ACPI tables!\n");
    }


    /*
    MADT* madt = getMADT();
    if (madt != 0) {
        uint32_t length = madt->header.length;
        char* entry = (char*)madt->entries;
        char* end = (char*)(madt) + length;
        for (; entry < end; entry += ((MADT_entry*)entry)->length) {
            MADT_entry* e = (MADT_entry*)(entry);
            lprintf("MADT type %h length", e->type, e->length);
            if (e->type == MADT_LAPIC_entry_type) {
                MADT_LAPIC_entry* p = (MADT_LAPIC_entry*)e;
                lprintf(" -> Local APIC CPU %h APIC ID %h flags %h\n", p->cpu_id, p->apic_id, p->flags);
            } else {
                lprintf("\n");
            }
        }
    }
    */
}

void start_cpus()
{
    extern char _cpuinit_start;
    extern char _cpuinit_end;
    
    kernel_cpu_init = (void*)syscall(SYSCALL_CONFIGURE_SYSTEM, 3, 0, 0).value;
    lprintf("CPUINIT %h %h %h\n", &_cpuinit_start, &_cpuinit_end, kernel_cpu_init);

    lprintf("Bringing up CPU...\n");
    syscall(SYSCALL_CONFIGURE_SYSTEM, 2, 1, 0);
    //for (int i = 0; i < 100000; ++i)
    //    asm volatile ("");

    syscall(SYSCALL_CONFIGURE_SYSTEM, 4, 50, 0);

    uint32_t vector = (uint32_t)(&_cpuinit_start) >> 12;
    syscall(SYSCALL_CONFIGURE_SYSTEM, 2, 2, vector);

    for (int i = 0; i < 20; ++i)
        syscall(SYSCALL_CONFIGURE_SYSTEM, 4, 100, 0);

    syscall(SYSCALL_CONFIGURE_SYSTEM, 2, 3, 2 | (50 << 8));

    syscall(SYSCALL_CONFIGURE_SYSTEM, 2, 3, 2 | (100 << 8));
}