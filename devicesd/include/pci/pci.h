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

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <uacpi/event.h>
#include <vector.h>

void init_pci();

uint8_t pci_readb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset);
uint16_t pci_readw(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset);
uint32_t pci_readl(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset);

void pci_writeb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint8_t val);
void pci_writew(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset,
                uint16_t val);
void pci_writel(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset,
                uint32_t val);

#define VENDOR_ID_NO_DEVICE 0xFFFF

#define HEADER_TYPE_MASK     0x7f;
#define HEADER_MULTIFUNCTION 0x80;

struct PCIGroupInterruptEntry {
    uint8_t bus;
    uint8_t device;
    uint8_t pin;

    bool active_low : 1;
    bool level_trigger : 1;

    uint32_t gsi;
};

enum PCIGroupType {
    PCIGroupECAM,
    PCIGroupLegacy,
};

struct PCIEcamDescriptor {
    uint64_t base_addr;
    // Pointer to the bus 0 of the group (even if it doesn't exist)
    void *base_ptr;
};

struct PCILegacyDescriptor {
    uint16_t config_addr_io;
    uint16_t config_data_io;
};

struct PCIHostBridge {
    struct PCIHostBridge *next;

    struct PCIEcamDescriptor ecam;
    struct PCILegacyDescriptor legacy;

    unsigned group_number;
    unsigned start_bus_number;
    unsigned end_bus_number;

    bool has_io;
    bool has_ecam;

    VECTOR(struct PCIGroupInterruptEntry) interrupt_entries;
};

struct PCIHostBridge *pci_host_bridge_find(unsigned group_number);

struct PCIDevice {
    uint16_t group;
    uint8_t bus;
    uint8_t device;
    uint8_t function;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class_code;
    uint8_t subclass;

    struct PCIDevice *associated_bridge;

    int pcie : 1;
    int downstream : 1;
};

struct LegacyAddr {
    uint32_t data_offset;
    uint16_t port;
};

struct PCIDevicePtr {
    enum PCIGroupType type;
    union {
        uint32_t *ecam_window;
        struct LegacyAddr io_addr;
    };
};

int pcicdevice_compare(const void *a, const void *b);

VECTOR_TYPEDEF(struct PCIDevice *, PCIDeviceVector);
extern PCIDeviceVector pci_devices;

#define PCI_FUNCTIONS       8
#define PCI_DEVICES_PER_BUS 32

uint32_t pci_read_register(struct PCIDevicePtr *s, unsigned register);
void pci_write_register(struct PCIDevicePtr *s, unsigned register, uint32_t value);
int fill_device(struct PCIDevicePtr *s, struct PCIHostBridge *g, int bus, int device, int function);

extern bool pci_fully_working;
int fill_device_early(struct PCIDevicePtr *s, int bus, int device, int function);

inline uint16_t pci_read_word(struct PCIDevicePtr *s, unsigned offset)
{
    uint32_t reg = pci_read_register(s, offset >> 2);
    return offset & 0x02 ? reg >> 16 : reg;
}

inline uint8_t pci_header_type(struct PCIDevicePtr *s)
{
    uint8_t r = pci_read_register(s, 0x3) >> 16;
    return r & HEADER_TYPE_MASK;
}

inline bool pci_multifunction(struct PCIDevicePtr *s)
{
    uint8_t r = pci_read_register(s, 0x3) >> 16;
    return r & HEADER_MULTIFUNCTION;
}

inline uint32_t pci_vendor_id(struct PCIDevicePtr *s)
{
    uint32_t r = pci_read_register(s, 0);
    return r & 0xffff;
}

inline uint32_t pci_device_id(struct PCIDevicePtr *s)
{
    uint32_t r = pci_read_register(s, 0);
    return r >> 16;
}

inline uint8_t pci_class_code(struct PCIDevicePtr *s) { return pci_read_register(s, 2) >> 24; }
inline uint8_t pci_subclass(struct PCIDevicePtr *s) { return pci_read_register(s, 2) >> 16; }
inline bool pci_no_device(struct PCIDevicePtr *s)
{
    return pci_vendor_id(s) == VENDOR_ID_NO_DEVICE;
}
inline uint8_t pci_secondary_bus(struct PCIDevicePtr *s) { return pci_read_register(s, 0x6) >> 8; }

inline uint16_t pcie_status(struct PCIDevicePtr *s)
{
    return (pci_read_register(s, 1) >> 16) & 0xffff;
}

inline int pcie_capability_pointer(struct PCIDevicePtr *s)
{
    if (!(pcie_status(s) & 0x10)) {
        return 0;
    }

    // TODO: PCI-to-CardBus Bridge ???
    return pci_read_register(s, 0x34/4) & 0xfc;
}
