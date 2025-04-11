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
#include <exceptions.hh>
#include <kern_logger/kern_logger.hh>
#include <lib/splay_tree_map.hh>
#include <string.h>
#include <types.hh>
#include <utils.hh>
#include <errno.h>

using namespace kernel::log;

struct ACPITable {
    u64 phys_addr;
    u64 length;
};

using ACPISignature = u32;
klib::splay_tree_map<ACPISignature, ACPITable> acpi_tables;

u64 get_table(u32 signature) { return acpi_tables.get_copy_or_default(signature).phys_addr; }

bool enumerate_tables_from_rsdt(u64 rsdt_desc_phys)
{
    if (rsdt_desc_phys == 0) {
        serial_logger.printf("Error: RSDT address is 0\n");
        return false;
    }

    RSDT rsdt_virt;
    copy_from_phys((u64)rsdt_desc_phys, &rsdt_virt, sizeof(rsdt_virt));

    auto size = rsdt_virt.h.length;
    if (memcmp(rsdt_virt.h.signature, "RSDT", 4) != 0) {
        char buf[5];
        memcpy(buf, rsdt_virt.h.signature, 4);
        buf[4] = 0;

        serial_logger.printf("Error: RSDT signature mismatch: %s\n", buf);
        return false;
    }

    size_t entries = (size - sizeof(ACPISDTHeader)) / 4;
    klib::vector<u32> pointers;
    if (!pointers.resize(entries)) {
        serial_logger.printf("Failed to allocate memory for ACPI entries!");
        return false;
    }
    copy_from_phys(rsdt_desc_phys + sizeof(ACPISDTHeader), pointers.data(), entries * 4);

    for (size_t i = 0; i < entries; ++i) {
        ACPISDTHeader header;
        copy_from_phys((u64)pointers[i], &header, sizeof(header));

        acpi_tables.insert({*(u32 *)header.signature, {pointers[i], header.length}});
    }

    return true;
}

bool enumerate_tables_from_xsdt(u64 xsdt_desc_phys)
{
    if (xsdt_desc_phys == 0) {
        serial_logger.printf("Error: XSDT address is 0\n");
        return false;
    }

    XSDT xsdt_virt;
    copy_from_phys((u64)xsdt_desc_phys, &xsdt_virt, sizeof(xsdt_virt));

    auto size = xsdt_virt.h.length;
    if (memcmp(xsdt_virt.h.signature, "XSDT", 4) != 0) {
        char buf[5];
        memcpy(buf, xsdt_virt.h.signature, 4);
        buf[4] = 0;

        serial_logger.printf("Error: XSDT signature mismatch: %s\n", buf);
        return false;
    }

    size_t entries = (size - sizeof(ACPISDTHeader)) / 8;
    klib::vector<u64> pointers;
    if (!pointers.resize(entries)) {
        serial_logger.printf("Failed to allocate memory for ACPI entries!");
        return false;
    }


    copy_from_phys(xsdt_desc_phys + sizeof(ACPISDTHeader), pointers.data(), entries * 8);

    for (size_t i = 0; i < entries; ++i) {
        ACPISDTHeader header;
        copy_from_phys(pointers[i], &header, sizeof(header));

        acpi_tables.insert({*(u32 *)header.signature, {pointers[i], header.length}});
    }

    return true;
}

bool enumerate_tables(u64 rsdt_desc_phys)
{
    serial_logger.printf("Enumerating ACPI tables...");

    RSDP_descriptor desc;
    copy_from_phys(rsdt_desc_phys, &desc, sizeof(desc));

    // Wisdom from someone on the internet: ACPI checksums are broken on half of the real hardware
    // Don't verify them

    if (memcmp(desc.signature, "RSD PTR ", 8) != 0) {
        serial_logger.printf(" Error: RSDP signature mismatch\n");
        return false;
    }

    if (desc.revision > 1) {
        RSDP_descriptor20 desc20;
        copy_from_phys(rsdt_desc_phys, &desc20, sizeof(desc20));

        bool enumerated_xsdt = enumerate_tables_from_xsdt(desc20.xsdt_address);
        if (enumerated_xsdt)
            return true;
    }

    bool b = enumerate_tables_from_rsdt(desc.rsdt_address);
    if (!b)
        serial_logger.printf(" Error: Failed to enumerate ACPI tables\n");

    serial_logger.printf(" Success!\n");
    return b;
}