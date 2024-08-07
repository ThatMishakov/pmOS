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
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <pci/pci.h>
#include <pmos/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector.h>
#include <pmos/ipc.h>
#include <string.h>

PCIDeviceVector pci_devices = VECTOR_INIT;

void check_bus(struct PCIGroup *g, uint8_t bus);

void check_function(struct PCIGroup *g, uint8_t bus, uint8_t device, uint8_t function)
{
    void *c = pcie_config_space_device(g, bus, device, function);
    printf("!! PCI group %i bus %i device %i function %i vendor %x device %x", g->group_number, bus,
           device, function, pcie_vendor_id(c), pcie_device_id(c));
    printf("  -> class %x subclass %x\n", pcie_class_code(c), pcie_subclass(c));

    uint8_t class    = pcie_class_code(c);
    uint8_t subclass = pcie_subclass(c);
    if (class == 0x06 && subclass == 0x04) { // Host bridge
        uint8_t secondary_bus = pci_secondary_bus(c);
        printf("PCI secondary bus %x\n", secondary_bus);
        check_bus(g, secondary_bus);
    }

    struct PCIDevice d = {
        .group      = g->group_number,
        .bus        = bus,
        .device     = device,
        .function   = function,
        .vendor_id  = pcie_vendor_id(c),
        .device_id  = pcie_device_id(c),
        .class_code = pcie_class_code(c),
        .subclass   = pcie_subclass(c),
    };

    int result = 0;
    VECTOR_PUSH_BACK_CHECKED(pci_devices, d, result);

    if (result != 0) {
        fprintf(stderr, "Error: Could not allocate PCIDevice: %i\n", errno);
    }
}

void check_device(struct PCIGroup *g, uint8_t bus, uint8_t device)
{
    void *c            = pcie_config_space_device(g, bus, device, 0);
    uint16_t vendor_id = pcie_vendor_id(c);
    if (vendor_id == VENDOR_ID_NO_DEVICE)
        return;

    check_function(g, bus, device, 0);
    if (pcie_multifunction(c)) {
        for (int function = 1; function < PCI_FUNCTIONS; ++function) {
            union PCIConfigSpace *c = pcie_config_space_device(g, bus, device, function);
            uint16_t vendor_id      = pcie_vendor_id(c);
            if (vendor_id != VENDOR_ID_NO_DEVICE)
                check_function(g, bus, device, function);
        }
    }
}

void check_bus(struct PCIGroup *g, uint8_t bus)
{
    for (int i = 0; i < PCI_DEVICES_PER_BUS; ++i) {
        check_device(g, bus, i);
    }
}

void check_all_buses(struct PCIGroup *g)
{
    uint8_t bus = g->start_bus_number;
    void *c     = pcie_config_space_device(g, bus, 0, 0);
    printf("%lx\n", c);
    printf("%x\n", pci_read_register(c, 0));
    if (!pcie_multifunction(c)) {
        check_bus(g, bus);
    } else {
        printf("Multifunction\n");
        for (int function = 0; function < PCI_FUNCTIONS; ++function) {
            union PCIConfigSpace *c = pcie_config_space_device(g, bus, 0, function);
            uint16_t vendor_id      = pcie_vendor_id(c);
            if (vendor_id == VENDOR_ID_NO_DEVICE)
                break;

            check_bus(g, function);
        }
    }
}

struct PCIGroup *groups = NULL;

void init_pci()
{
    printf("Iitializing PCI...\n");

    MCFG *t = (MCFG *)get_table("MCFG", 0);
    if (!t) {
        printf("Warning: Could not find MCFG table...\n");
        return;
    }

    int c = MCFG_list_size(t);
    for (int i = 0; i < c; ++i) {
        struct PCIGroup *g = malloc(sizeof(*g));
        if (!g) {
            fprintf(stderr, "Error: Could not allocate PCIGroup: %i\n", errno);
        }

        MCFGBase *b = &t->structs_list[i];
        printf("Table %i base addr %lx group %i start %i end %i\n", i, b->base_addr,
               b->group_number, b->start_bus_number, b->end_bus_number);

        assert(b->start_bus_number <= b->end_bus_number);
        uint64_t ecam_base = b->base_addr;
        uint64_t size      = pcie_config_space_size(b->start_bus_number, b->end_bus_number);
        uint64_t start     = pcie_config_space_start(ecam_base, b->start_bus_number);

        uint64_t phys_start = start;
        uint64_t phys_end   = phys_start + size;

        // Align to page just in case
        static const uint64_t page_mask = ~((uint64_t)PAGE_SIZE - 1);
        phys_start &= page_mask;
        phys_end = (phys_end + PAGE_SIZE - 1) & page_mask;

        printf("Mapping %lx size %lx\n", phys_start, phys_end - phys_start);
        mem_request_ret_t t = create_phys_map_region(0, NULL, phys_end - phys_start,
                                                     PROT_READ | PROT_WRITE, (void *)phys_start);
        if (t.result != SUCCESS) {
            fprintf(stderr, "Error: could not map PCIe memory: %lx\n", t.result);
            free(g);
            continue;
        }
        printf("<<<< %lx\n", t.virt_addr);

        void *ptr = (char *)t.virt_addr + (ecam_base % PAGE_SIZE) - (start - ecam_base);

        g->next = groups;
        groups  = g;

        g->base_addr        = ecam_base;
        g->base_ptr         = ptr;
        g->group_number     = b->group_number;
        g->start_bus_number = b->start_bus_number;
        g->end_bus_number   = b->end_bus_number;

        check_all_buses(g);
    }

    VECTOR_SORT(pci_devices, pcicdevice_compare);
}

struct PCIGroup *pci_group_find(unsigned group_number)
{
    for (struct PCIGroup *g = groups; g; g = g->next) {
        if (g->group_number == group_number)
            return g;
    }

    return NULL;
}

int pcicdevice_compare(const void *aa, const void *bb)
{
    const struct PCIDevice *a = aa, *b = bb;

    if (a->group != b->group)
        return a->group - b->group;
    if (a->bus != b->bus)
        return a->bus - b->bus;
    if (a->device != b->device)
        return a->device - b->device;
    return a->function - b->function;
}

void request_pci_device(Message_Descriptor *desc, IPC_Request_PCI_Device *d)
{
    int error = 0;

    struct PCIGroup *g = pci_group_find(d->group);
    if (!g) {
        error = -ENOENT;
        goto err;
    }

    // Calculate the offset of the device in the configuration space
    const unsigned long offset = (d->bus << 20) | (d->device << 15) | (d->function << 12);

    IPC_Request_PCI_Device_Reply reply_success = {
        .type        = IPC_Request_PCI_Device_Reply_NUM,
        .flags       = 0,
        .type_error  = IPC_PCI_ACCESS_TYPE_MMIO,
        .base_address = g->base_addr + offset,
    };

    result_t result = send_message_port(d->reply_port, sizeof(reply_success), (char *)&reply_success);
    if (result != 0)
        printf("Failed to send message in request_pci_device: %li\n", result);
    return;
err:
    IPC_Request_PCI_Device_Reply reply = {
        .type      = IPC_Request_PCI_Device_Reply_NUM,
        .flags     = 0,
        .type_error = error,
        .base_address = 0,
    };

    send_message_port(d->reply_port, sizeof(reply), (char *)&reply);
}

void request_pci_devices(Message_Descriptor *desc, IPC_Request_PCI_Devices *d)
{
    size_t requests = (desc->size - sizeof(IPC_Request_PCI_Devices)) / sizeof(struct PCIDevice);

    VECTOR(struct IPC_PCIDeviceLocation) devices = VECTOR_INIT;

    struct PCIDevice dd;
    int result = 0;
    VECTOR_FOREACH(pci_devices, dd) {
        for (int i = 0; i < requests; ++i) {
            if (d->devices[i].vendor_id != 0xffff &&
                d->devices[i].vendor_id != dd.vendor_id)
                continue;

            if (d->devices[i].device_id != 0xffff &&
                d->devices[i].device_id != dd.device_id)
                continue;

            if (d->devices[i].class_code != 0xff &&
                d->devices[i].class_code != dd.class_code)
                continue;

            if (d->devices[i].subclass != 0xff &&
                d->devices[i].subclass != dd.subclass)
                continue;

            // if (d->devices[i].prog_if != 0xff &&
            //     d->devices[i].prog_if != dd.prog_if)
            //     continue;

            VECTOR_PUSH_BACK_CHECKED(devices, ((struct IPC_PCIDeviceLocation){
                                          .group    = dd.group,
                                          .bus      = dd.bus,
                                          .device   = dd.device,
                                          .function = dd.function,
                                      }), result);
            if (result != 0)
                goto error;
            break;
        }
    }

    size_t reply_size = sizeof(IPC_Request_PCI_Devices_Reply) + devices.size * sizeof(struct IPC_PCIDeviceLocation);
    IPC_Request_PCI_Devices_Reply *reply = alloca(reply_size);

    reply->type = IPC_Request_PCI_Devices_NUM;
    reply->flags = 0;
    reply->result_num_of_devices = devices.size;
    memcpy(reply->devices, devices.data, devices.size * sizeof(struct IPC_PCIDeviceLocation));

    send_message_port(d->reply_port, reply_size, (char *)reply);
    return;
error:
    int e = -errno;

    fprintf(stderr, "Error: Could not allocate IPC_PCIDeviceLocation: %i\n", errno);

    IPC_Request_PCI_Devices_Reply reply_e = {
        .type                = IPC_Request_PCI_Devices_NUM,
        .flags               = 0,
        .result_num_of_devices = e,
    };

    send_message_port(d->reply_port, sizeof(reply_e), (char *)&reply_e);
}