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

#ifndef PCI_H
#define PCI_H
#include <stdint.h>
#include <stdbool.h>
#include <vector.h>

void init_pci();

uint8_t pci_readb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset);
uint16_t pci_readw(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset);
uint32_t pci_readl(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset);

void pci_writeb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint8_t val);
void pci_writew(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint16_t val);
void pci_writel(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint32_t val);

#define VENDOR_ID_NO_DEVICE 0xFFFF

#define HEADER_TYPE_MASK 0x7f;
#define HEADER_MULTIFUNCTION 0x80;

struct PCIGroup {
    struct PCIGroup *next;
    uint64_t base_addr;

    // Pointer to the bus 0 of the group (even if it doesn't exist)
    void *base_ptr;

    unsigned group_number;
    unsigned start_bus_number;
    unsigned end_bus_number;
};

struct PCIDevice {
    uint16_t group;
    uint8_t bus;
    uint8_t device;
    uint8_t function;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class_code;
    uint8_t subclass;
};

int pcicdevice_compare(const void *a, const void *b);

VECTOR_TYPEDEF(struct PCIDevice, PCIDeviceVector);
extern PCIDeviceVector pci_devices;

inline unsigned long pcie_config_space_size(unsigned pci_start, unsigned pci_end)
{
    // https://wiki.osdev.org/PCI#Configuration_Space_Access_Mechanism_.231
    // With PCI, the registers are 256 bytes, with PCIe it's expanded to 4096 bytes
    // Thus, bus number is bits 27-20 of the pointer
    return (pci_end - pci_start + 1) << 20;
}

inline uint64_t pcie_config_space_start(uint64_t base, unsigned pci_start)
{
    return base + (pci_start << 20);
}

inline void *pcie_config_space_device(struct PCIGroup * group, unsigned bus, unsigned device, unsigned function)
{
    // config_space_virt is expected to point to the bus number 0, even if it is not mapped
    void * config_space_virt = group->base_ptr;

    // https://wiki.osdev.org/PCI_Express
    const unsigned long offset = (bus << 20) | (device << 15) | (function << 12);

    return (void *)((char *)config_space_virt + offset);
}

#define PCI_FUNCTIONS 8
#define PCI_DEVICES_PER_BUS 32

inline uint32_t pci_read_register(void *s, int id)
{
    volatile uint32_t *r = (volatile uint32_t *)s;
    return r[id];
}

inline uint8_t pcie_header_type(void *s)
{
    uint8_t r = pci_read_register(s, 0x3) >> 16;
    return r & HEADER_TYPE_MASK;
}

inline bool pcie_multifunction(void *s)
{
    uint8_t r = pci_read_register(s, 0x3) >> 16;
    return r & HEADER_MULTIFUNCTION;
}

inline uint32_t pcie_vendor_id(void *s)
{
    uint32_t r = pci_read_register(s, 0);
    return r & 0xffff;
}

inline uint32_t pcie_device_id(void *s)
{
    uint32_t r = pci_read_register(s, 0);
    return r >> 16;
}

inline uint8_t pcie_class_code(void *s)
{
    return pci_read_register(s, 2) >> 24;
}

inline uint8_t pcie_subclass(void *s)
{
    return pci_read_register(s, 2) >> 16;
}

inline bool pci_no_device(void *s)
{
    return pcie_vendor_id(s) == VENDOR_ID_NO_DEVICE;
}

inline uint8_t pci_secondary_bus(void *s)
{
    return pci_read_register(s, 0x6) >> 8;
}

#endif