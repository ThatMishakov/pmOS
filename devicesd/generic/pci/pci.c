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
#include <alloca.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <io.h>
#include <limits.h>
#include <pci/pci.h>
#include <pmos/ipc.h>
#include <pmos/memory.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uacpi/event.h>
#include <uacpi/namespace.h>
#include <uacpi/resources.h>
#include <uacpi/tables.h>
#include <uacpi/utilities.h>
#include <vector.h>

PCIDeviceVector pci_devices = VECTOR_INIT;

void check_bus(struct PCIHostBridge *g, uint8_t bus, struct PCIDevice *parent_bridge,
               uacpi_namespace_node *bridge_node);

struct DeviceSearchCtx {
    uint64_t addr;
    uacpi_namespace_node *out_node;
};

uacpi_iteration_decision find_pci_device(void *param, uacpi_namespace_node *node, uint32_t depth)
{
    struct DeviceSearchCtx *ctx = param;
    uint64_t addr               = 0;
    (void)depth;

    uacpi_status ret = uacpi_eval_integer(node, "_ADR", UACPI_NULL, &addr);
    if (ret != UACPI_STATUS_OK && ret != UACPI_STATUS_NOT_FOUND)
        return UACPI_ITERATION_DECISION_CONTINUE;

    if (addr == ctx->addr) {
        ctx->out_node = node;
        return UACPI_ITERATION_DECISION_BREAK;
    }

    return UACPI_ITERATION_DECISION_CONTINUE;
}

void parse_interrupt_table(struct PCIHostBridge *g, int bus, uacpi_pci_routing_table *pci_routes);

void check_function(struct PCIHostBridge *g, uint8_t bus, uint8_t device, uint8_t function,
                    struct PCIDevice *parent_bridge, uacpi_namespace_node *node)
{
    struct PCIDevicePtr p;
    fill_device(&p, g, bus, device, function);
    printf("!! PCI group %i bus %i device %i function %i vendor %x device %x", g->group_number, bus,
           device, function, pci_vendor_id(&p), pci_device_id(&p));
    printf("  -> class %x subclass %x\n", pci_class_code(&p), pci_subclass(&p));

    struct PCIDevice *d = malloc(sizeof(*d));
    if (!d) {
        fprintf(stderr, "Error: Could not allocate PCIDevice: %i\n", errno);
        return;
    }

    *d = (struct PCIDevice) {
        .group             = g->group_number,
        .bus               = bus,
        .device            = device,
        .function          = function,
        .vendor_id         = pci_vendor_id(&p),
        .device_id         = pci_device_id(&p),
        .class_code        = pci_class_code(&p),
        .subclass          = pci_subclass(&p),
        .associated_bridge = parent_bridge,
    };

    // Capabilities
    uint16_t cap = pcie_capability_pointer(&p);
    while (cap) {
        uint16_t r = pci_read_word(&p, cap);

        uint8_t id = r & 0xff;
        cap        = (r >> 8) & 0xfc;
        switch (id) {
        case 0x1:
            // printf("PCI Power Management Interface\n");
            break;
        case 0x4:
            // printf("Slot Identification\n");
            break;
        case 0x5:
            // printf("MSI Capability\n");
            break;
        case 0x6:
            // printf("CompactPCI Hot Swap\n");
            break;
        case 0x7:
            // printf("PCI-X Capability\n");
            break;
        case 0x8:
            // printf("HyperTransport Capability\n");
            break;
        case 0x9:
            // printf("Vendor Specific Capability\n");
            break;
        case 0x10: {
            printf("PCI Express Capability ");
            d->pcie = true;

            uint16_t r = pci_read_word(&p, cap + 2);
            uint8_t v  = r & 0xf;
            printf("version %i.%i ", v >> 4, v & 0xf);

            uint16_t dd = (r >> 4) & 0x1f;
            printf("device Type %i\n", dd);
            d->downstream = dd == 0x4 || dd == 0x6 || dd == 0x8;
        } break;
        case 0x11:
            // printf("MSI-X Capability\n");
            break;
        default:
            // printf("Unknown (%02X)\n", id);
            break;
        }
    }

    int result = 0;
    VECTOR_PUSH_BACK_CHECKED(pci_devices, d, result);

    if (result != 0) {
        fprintf(stderr, "Error: Could not allocate PCIDevice: %i\n", errno);
        free(d);
    }

    uint8_t class    = pci_class_code(&p);
    uint8_t subclass = pci_subclass(&p);
    if (class == 0x06 && subclass == 0x04) { // Host bridge
        struct DeviceSearchCtx ctx = {
            .addr     = d->device << 16 | d->function,
            .out_node = NULL,
        };
        uacpi_namespace_for_each_child(node, find_pci_device, UACPI_NULL, UACPI_OBJECT_DEVICE_BIT,
                                       UACPI_MAX_DEPTH_ANY, &ctx);

        uint8_t secondary_bus = pci_secondary_bus(&p);
        printf("PCI secondary bus %x\n", secondary_bus);

        uacpi_pci_routing_table *pci_routes;
        uacpi_status ret = uacpi_get_pci_routing_table(ctx.out_node, &pci_routes);
        if (ret != UACPI_STATUS_OK) {
            fprintf(stderr, "Warning: Could not get PCI routing for root bridge bus %0x: %i\n", bus,
                    ret);
        } else {
            parse_interrupt_table(g, secondary_bus, pci_routes);
            uacpi_free_pci_routing_table(pci_routes);
        }

        check_bus(g, secondary_bus, d, ctx.out_node);
    }
}

void check_device(struct PCIHostBridge *g, uint8_t bus, uint8_t device,
                  struct PCIDevice *parent_bridge, uacpi_namespace_node *parent_node)
{
    struct PCIDevicePtr p;
    fill_device(&p, g, bus, device, 0);
    uint16_t vendor_id = pci_vendor_id(&p);
    if (vendor_id == VENDOR_ID_NO_DEVICE)
        return;

    check_function(g, bus, device, 0, parent_bridge, parent_node);
    if (pci_multifunction(&p)) {
        for (int function = 1; function < PCI_FUNCTIONS; ++function) {
            struct PCIDevicePtr p;
            fill_device(&p, g, bus, device, function);
            uint16_t vendor_id = pci_vendor_id(&p);
            if (vendor_id != VENDOR_ID_NO_DEVICE)
                check_function(g, bus, device, function, parent_bridge, parent_node);
        }
    }
}

void check_bus(struct PCIHostBridge *g, uint8_t bus, struct PCIDevice *parent_bridge,
               uacpi_namespace_node *node)
{
    int devices = PCI_DEVICES_PER_BUS;

    if (parent_bridge && parent_bridge->downstream) {
        // This is supposedly needed for Raspberry Pi
        // I'm not sure if this is correct
        // devices = 1;
        printf("Downstream bridge\n");
    }

    for (int i = 0; i < devices; ++i) {
        check_device(g, bus, i, parent_bridge, node);
    }
}

void parse_interrupt_table(struct PCIHostBridge *g, int bus, uacpi_pci_routing_table *pci_routes)
{
    for (size_t i = 0; i < pci_routes->num_entries; ++i) {
        uacpi_pci_routing_table_entry *entry = &pci_routes->entries[i];

        struct PCIGroupInterruptEntry e = {
            .bus           = bus,
            .device        = entry->address >> 16,
            .pin           = entry->pin,
            .active_low    = true,
            .level_trigger = true,
            .gsi           = entry->index,
        };

        if (entry->source) {
            if (entry->index != 0) {
                printf("Warning: Unexpected index: %x\n", entry->index);
                return;
            }

            uacpi_resources *resources;
            uacpi_status ret = uacpi_get_current_resources(entry->source, &resources);
            if (ret != UACPI_STATUS_OK) {
                fprintf(stderr, "Warning: Could not get resources for source: %i\n", ret);
                return;
            }

            switch (resources->entries[0].type) {
            case UACPI_RESOURCE_TYPE_IRQ: {
                uacpi_resource_irq *irq = &resources->entries[0].irq;
                if (irq->num_irqs < 1) {
                    fprintf(stderr, "Warning: Unexpected number of IRQs: %i\n", irq->num_irqs);
                    break;
                }
                e.gsi = irq->irqs[0];

                if (irq->triggering == UACPI_TRIGGERING_EDGE)
                    e.level_trigger = false;
                if (irq->polarity == UACPI_POLARITY_ACTIVE_HIGH)
                    e.active_low = false;
            } break;
            case UACPI_RESOURCE_TYPE_EXTENDED_IRQ: {
                uacpi_resource_extended_irq *irq = &resources->entries[0].extended_irq;
                if (irq->num_irqs < 1) {
                    fprintf(stderr, "Warning: Unexpected number of IRQs: %i\n", irq->num_irqs);
                    break;
                }
                e.gsi = irq->irqs[0];

                if (irq->triggering == UACPI_TRIGGERING_EDGE)
                    e.level_trigger = false;
                if (irq->polarity == UACPI_POLARITY_ACTIVE_HIGH)
                    e.active_low = false;
            } break;
            default:
                fprintf(stderr, "Warning: Unexpected resource type: %i\n",
                        resources->entries[0].type);
                break;
            }

            uacpi_free_resources(resources);
        }

        // printf("PCI interrupt: BUS %#x DEVICE %#x PIN %#x GSI %u %s %s\n", e.bus, e.device,
        // e.pin, e.gsi, e.active_low ? "Active Low" : "Active High", e.level_trigger ? "Level" :
        // "Edge");
        int ret = 0;
        VECTOR_PUSH_BACK_CHECKED(g->interrupt_entries, e, ret);
        if (ret != 0) {
            fprintf(stderr, "Error: Could not allocate PCIGroupInterruptEntry: %i\n", errno);
        }
    }
}

void check_root(struct PCIHostBridge *g, int bus, uacpi_namespace_node *node)
{
    uacpi_pci_routing_table *pci_routes;
    uacpi_status ret = uacpi_get_pci_routing_table(node, &pci_routes);
    if (ret != UACPI_STATUS_OK) {
        fprintf(stderr, "Warning: Could not get PCI routing for root bridge bus %0x: %i\n", bus,
                ret);
    } else {
        parse_interrupt_table(g, bus, pci_routes);
        uacpi_free_pci_routing_table(pci_routes);
    }

    struct PCIDevicePtr p;
    fill_device(&p, g, bus, 0, 0);

    if (!pci_multifunction(&p)) {
        check_bus(g, bus, NULL, node);
    } else {
        printf("Root %i Multifunction\n", bus);
        for (int function = 0; function < PCI_FUNCTIONS; ++function) {
            struct PCIDevicePtr p;
            fill_device(&p, g, bus, 0, function);
            uint16_t vendor_id = pci_vendor_id(&p);
            if (vendor_id == VENDOR_ID_NO_DEVICE)
                break;

            check_bus(g, function, NULL, node);
        }
    }
}

struct PCIHostBridge *bridges = NULL;

static unsigned long pcie_config_space_size(unsigned pci_start, unsigned pci_end)
{
    // https://wiki.osdev.org/PCI#Configuration_Space_Access_Mechanism_.231
    // With PCI, the registers are 256 bytes, with PCIe it's expanded to 4096 bytes
    // Thus, bus number is bits 27-20 of the pointer
    return (pci_end - pci_start + 1) << 20;
}

static uint64_t pcie_config_space_start(uint64_t base, unsigned pci_start)
{
    return base + (pci_start << 20);
}

// void init_pci()
// {
//     printf("Initializing PCI...\n");

//     MCFG *t = (MCFG *)get_table("MCFG", 0);
//     if (!t) {
//         printf("Warning: Could not find MCFG table... Falling back to legacy init\n");
//         printf("(not implemented)\n");
//         return;
//     } else {
//         int c = MCFG_list_size(t);
//         for (int i = 0; i < c; ++i) {
//             struct PCIGroup *g = calloc(sizeof(*g), 1);
//             if (!g) {
//                 fprintf(stderr, "Error: Could not allocate PCIGroup: %i\n", errno);
//             }

//             MCFGBase *b = &t->structs_list[i];
//             printf("Table %i base addr %" PRIx64 " group %i start %i end %i\n", i, b->base_addr,
//                    b->group_number, b->start_bus_number, b->end_bus_number);

//             assert(b->start_bus_number <= b->end_bus_number);
//             uint64_t ecam_base = b->base_addr;
//             uint64_t size      = pcie_config_space_size(b->start_bus_number, b->end_bus_number);
//             uint64_t start     = pcie_config_space_start(ecam_base, b->start_bus_number);

//             uint64_t phys_start = start;
//             uint64_t phys_end   = phys_start + size;

//             // Align to page just in case
//             static const uint64_t page_mask = ~((uint64_t)PAGE_SIZE - 1);
//             phys_start &= page_mask;
//             phys_end = (phys_end + PAGE_SIZE - 1) & page_mask;

//             mem_request_ret_t t = create_phys_map_region(0, NULL, phys_end - phys_start,
//                                                          PROT_READ | PROT_WRITE, phys_start);
//             if (t.result != SUCCESS) {
//                 fprintf(stderr, "Error: could not map PCIe memory: %" PRIi64 "\n", t.result);
//                 free(g);
//                 continue;
//             }
//             printf("ECAM %lx %lx\n", t.virt_addr, phys_end - phys_start);
//             void *ptr = (char *)t.virt_addr + (ecam_base % PAGE_SIZE) - (start - ecam_base);

//             g->next = groups;
//             groups  = g;

//             g->group_type = PCIGroupECAM;
//             g->ecam       = (struct PCIEcamDescriptor) {
//                       .base_addr = ecam_base,
//                       .base_ptr  = ptr,
//             };
//             g->start_bus_number = b->start_bus_number;
//             g->end_bus_number   = b->end_bus_number;
//         }
//     }

//     VECTOR_SORT(pci_devices, pcicdevice_compare);
// }

// struct PCIGroup *get_group(int segment)
// {
//     for (struct PCIGroup *g = groups; g; g = g->next) {
//         if (g->group_number == (uint32_t)segment)
//             return g;
//     }

//     return NULL;
// }

int interrupt_entry_compare(const void *l, const void *r)
{
    struct PCIGroupInterruptEntry const *a = l, *b = r;

    if (a->bus != b->bus)
        return a->bus - b->bus;
    if (a->device != b->device)
        return a->device - b->device;
    if (a->pin != b->pin)
        return a->pin - b->pin;

    return 0;
}

void enumerate_pci_bus(struct PCIHostBridge *g, int segment, int bus, uacpi_namespace_node *node)
{
    printf("Enumerating PCI bus %i\n", bus);
    check_root(g, bus, node);

    VECTOR_SORT(g->interrupt_entries, interrupt_entry_compare);
}

uacpi_iteration_decision pci_enumerate_resources(void *ctx, uacpi_resource *resource)
{
    struct PCIHostBridge *b = ctx;
    uint64_t base, size, offset;

    switch (resource->type) {
    case UACPI_RESOURCE_TYPE_IO: {
        if (b->has_io)
            break;

        uacpi_resource_io *r   = &resource->io;
        uint16_t range_minimum = r->minimum;
        uint16_t length        = r->length;
        if (length < 8)
            break;

        b->legacy.config_addr_io = range_minimum;
        b->legacy.config_data_io = range_minimum + 8;
        b->has_io                = true;
    } break;

    default:
        // TODO I guess...
        break;
    }

    // printf("PCI Resource %i\n", resource->type);

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static bool find_ecam_mcfg(uint64_t *ecam_base_out, uint16_t segment, uint8_t start_bus,
                           uint8_t end_bus)
{
    bool result = false;
    assert(ecam_base_out);
    uacpi_table mcfg_table;
    uacpi_status status = uacpi_table_find_by_signature(ACPI_MCFG_SIGNATURE, &mcfg_table);
    if (status != UACPI_STATUS_OK)
        return false;
    struct acpi_mcfg *mcfg = mcfg_table.ptr;

    for (uint32_t i = 0, mcfg_size = (mcfg->hdr.length - offsetof(struct acpi_mcfg, entries)) /
                                     sizeof(struct acpi_mcfg_allocation);
         i < mcfg_size; ++i) {
        if (mcfg->entries[i].segment == segment && mcfg->entries[i].start_bus <= start_bus &&
            mcfg->entries[i].end_bus >= end_bus) {
            *ecam_base_out = mcfg->entries[i].address;
            result         = true;
            break;
        }
    }

    uacpi_table_unref(&mcfg_table);
    return result;
}

static void pci_setup_ecam(struct PCIHostBridge *b, uacpi_namespace_node *node)
{
    uint64_t ecam_base_0;
    bool have_ecam =
        find_ecam_mcfg(&ecam_base_0, b->group_number, b->start_bus_number, b->end_bus_number);
    if (have_ecam) {
        printf("devicesd: Found ECAM address in MCFG: %" PRIx64 "\n", ecam_base_0);
    } else {
        have_ecam = (uacpi_eval_integer(node, "_CBA", NULL, &ecam_base_0) == UACPI_STATUS_OK);
        if (have_ecam)
            printf("devicesd: Found ECAM address in _CBA: %" PRIx64 "\n", ecam_base_0);
    }

    if (have_ecam) {
        uint64_t size  = pcie_config_space_size(b->start_bus_number, b->end_bus_number);
        uint64_t start = pcie_config_space_start(ecam_base_0, b->start_bus_number);

        uint64_t phys_start = start;
        uint64_t phys_end   = phys_start + size;

        // Align to page just in case
        static const uint64_t page_mask = ~((uint64_t)PAGE_SIZE - 1);
        phys_start &= page_mask;
        phys_end = (phys_end + PAGE_SIZE - 1) & page_mask;

        mem_request_ret_t t = create_phys_map_region(0, NULL, phys_end - phys_start,
                                                     PROT_READ | PROT_WRITE, phys_start);
        if (t.result != SUCCESS) {
            fprintf(stderr, "devicesd: Error: could not map PCIe memory: %" PRIi64 "\n", t.result);
            return;
        }
        // printf("ECAM %lx %lx\n", t.virt_addr, phys_end - phys_start);
        void *ptr = (char *)t.virt_addr + (ecam_base_0 % PAGE_SIZE) - (start - ecam_base_0);
        
        b->ecam.base_addr = ecam_base_0;
        b->ecam.base_ptr = ptr;
        b->has_ecam = true;
    }
}

uacpi_iteration_decision pci_check_acpi_root(void *, uacpi_namespace_node *node, uint32_t /*depth*/)
{
    struct PCIHostBridge *b = calloc(sizeof(*b), 1);
    if (!b) {
        fprintf(stderr, "devicesd: Failed to enumerate host bridge, out of memory\n");
        return UACPI_ITERATION_DECISION_CONTINUE;
    }

    uint64_t seg = 0, bus = 0;

    uacpi_eval_integer(node, "_SEG", NULL, &seg);
    uacpi_eval_integer(node, "_BBN", NULL, &bus);
    b->group_number     = seg;
    b->start_bus_number = bus;
    b->end_bus_number   = 255;

    // uacpi_for_each_device_resource(node, "_CRS", pci_enumerate_resources, b);
    b->legacy.config_addr_io = 0xcf8;
    b->legacy.config_data_io = 0xcfc;
    b->has_io                = true;

    pci_setup_ecam(b, node);

    b->next = bridges;
    bridges = b;

    enumerate_pci_bus(b, seg, bus, node);

    return UACPI_ITERATION_DECISION_CONTINUE;
}

bool pci_fully_working = false;
void acpi_pci_init()
{
    static const char *pciRootIds[] = {
        "PNP0A03",
        "PNP0A08",
        NULL,
    };

    uacpi_find_devices_at(uacpi_namespace_get_predefined(UACPI_PREDEFINED_NAMESPACE_SB), pciRootIds,
                          pci_check_acpi_root, NULL);

    pci_fully_working = true;
}

struct PCIHostBridge *pci_host_bridge_find(unsigned group_number)
{
    for (struct PCIHostBridge *g = bridges; g; g = g->next) {
        if (g->group_number == group_number)
            return g;
    }

    return NULL;
}

int pcicdevice_compare(const void *aa, const void *bb)
{
    const struct PCIDevice *const *const a = aa, *const *const b = bb;

    if ((*a)->group != (*b)->group)
        return (*a)->group - (*b)->group;
    if ((*a)->bus != (*b)->bus)
        return (*a)->bus - (*b)->bus;
    if ((*a)->device != (*b)->device)
        return (*a)->device - (*b)->device;
    return (*a)->function - (*b)->function;
}

void request_pci_device(Message_Descriptor *desc, IPC_Request_PCI_Device *d)
{
    int error = 0;
    IPC_Request_PCI_Device_Reply reply;

    if (desc->size < sizeof(IPC_Request_PCI_Device)) {
        error = -EINVAL;
        goto err;
    }

    struct PCIHostBridge *g = pci_host_bridge_find(d->group);
    if (!g) {
        error = -ENOENT;
        goto err;
    }

    // Calculate the offset of the device in the configuration space
    const unsigned long offset = (d->bus << 20) | (d->device << 15) | (d->function << 12);

    IPC_Request_PCI_Device_Reply reply_success = {
        .type         = IPC_Request_PCI_Device_Reply_NUM,
        .flags        = 0,
        .type_error   = IPC_PCI_ACCESS_TYPE_MMIO,
        .base_address = g->ecam.base_addr + offset,
    };

    result_t result =
        send_message_port(d->reply_port, sizeof(reply_success), (char *)&reply_success);
    if (result != 0)
        printf("Failed to send message in request_pci_device: %li\n", result);
    return;
err:
    reply = (IPC_Request_PCI_Device_Reply) {
        .type         = IPC_Request_PCI_Device_Reply_NUM,
        .flags        = 0,
        .type_error   = error,
        .base_address = 0,
    };

    send_message_port(d->reply_port, sizeof(reply), (char *)&reply);
}

void request_pci_devices(Message_Descriptor *desc, IPC_Request_PCI_Devices *d)
{
    int e;
    size_t requests = (desc->size - sizeof(IPC_Request_PCI_Devices)) / sizeof(struct IPC_PCIDevice);
    VECTOR(struct IPC_PCIDeviceLocation) devices = VECTOR_INIT;

    struct PCIDevice *dd;
    int result = 0;
    VECTOR_FOREACH(pci_devices, dd)
    {
        for (size_t i = 0; i < requests; ++i) {
            if (d->devices[i].vendor_id != 0xffff && d->devices[i].vendor_id != dd->vendor_id)
                continue;

            if (d->devices[i].device_id != 0xffff && d->devices[i].device_id != dd->device_id)
                continue;

            if (d->devices[i].class_code != 0xff && d->devices[i].class_code != dd->class_code)
                continue;

            if (d->devices[i].subclass != 0xff && d->devices[i].subclass != dd->subclass)
                continue;

            // if (d->devices[i].prog_if != 0xff &&
            //     d->devices[i].prog_if != dd.prog_if)
            //     continue;

            VECTOR_PUSH_BACK_CHECKED(devices,
                                     ((struct IPC_PCIDeviceLocation) {
                                         .group    = dd->group,
                                         .bus      = dd->bus,
                                         .device   = dd->device,
                                         .function = dd->function,
                                     }),
                                     result);
            if (result != 0)
                goto error;
            break;
        }
    }

    size_t reply_size =
        sizeof(IPC_Request_PCI_Devices_Reply) + devices.size * sizeof(struct IPC_PCIDeviceLocation);
    IPC_Request_PCI_Devices_Reply *reply = alloca(reply_size);

    reply->type                  = IPC_Request_PCI_Devices_NUM;
    reply->flags                 = 0;
    reply->result_num_of_devices = devices.size;
    memcpy(reply->devices, devices.data, devices.size * sizeof(struct IPC_PCIDeviceLocation));

    send_message_port(d->reply_port, reply_size, (char *)reply);
    return;
error:
    e = -errno;

    fprintf(stderr, "Error: Could not allocate IPC_PCIDeviceLocation: %i\n", errno);

    IPC_Request_PCI_Devices_Reply reply_e = {
        .type                  = IPC_Request_PCI_Devices_NUM,
        .flags                 = 0,
        .result_num_of_devices = e,
    };

    send_message_port(d->reply_port, sizeof(reply_e), (char *)&reply_e);
}

struct PCIDevice *find_pci_device_descriptor(struct PCIHostBridge *g, uint8_t bus, uint8_t device,
                                             uint8_t function)
{
    struct PCIDevice *d;
    // TODO: bsearch
    VECTOR_FOREACH(pci_devices, d)
    {
        if (d->group == g->group_number && d->bus == bus && d->device == device &&
            d->function == function)
            return d;
    }

    return NULL;
}

int resolve_gsi_for(struct PCIHostBridge *g, uint8_t bus, uint8_t device, uint8_t function,
                    uint8_t pin, uint32_t *gsi, bool *active_low, bool *level_trig,
                    struct PCIDevice *d)
{
    struct PCIGroupInterruptEntry key = {
        .bus    = bus,
        .device = device,
        .pin    = pin,
    };

    struct PCIGroupInterruptEntry *e =
        bsearch(&key, g->interrupt_entries.data, g->interrupt_entries.size,
                sizeof(struct PCIGroupInterruptEntry), interrupt_entry_compare);
    if (e) {
        *gsi        = e->gsi;
        *active_low = e->active_low;
        *level_trig = e->level_trigger;
        printf("devicesd: PCI device %i:%i:%i:%i pin %i GSI %i\n", g->group_number, bus, device,
               function, pin, *gsi);
        return 0;
    }

    if (!d) {
        // Find PCI device
        d = find_pci_device_descriptor(g, bus, device, function);
        if (!d) {
            printf("devicesd: PCI device %i:%i:%i:%i not found\n", g->group_number, bus, device,
                   function);
            return -ENOENT;
        }
    }

    // Find the parent bridge
    struct PCIDevice *bridge = d->associated_bridge;
    if (!bridge) {
        printf("devicesd: PCI device %i:%i:%i:%i has no parent bridge\n", g->group_number, bus,
               device, function);
        return -ENOENT;
    }

    // Calculate pin of the parent bridge
    uint8_t bridge_pin = (device + pin) % 4;

    // printf("devicesd: PCI device %i:%i:%i:%i parent bridge %i:%i:%i:%i pin %i\n",
    // g->group_number, bus, device, function, bridge->group, bridge->bus, bridge->device,
    // bridge->function, bridge_pin);

    return resolve_gsi_for(g, bridge->bus, bridge->device, bridge->function, bridge_pin, gsi,
                           active_low, level_trig, bridge);
}

void request_pci_device_gsi(Message_Descriptor *desc, IPC_Request_PCI_Device_GSI *d)
{
    if (desc->size < sizeof(IPC_Request_PCI_Device_GSI)) {
        IPC_Request_PCI_Device_GSI_Reply reply = {
            .type   = IPC_Request_PCI_Device_GSI_Reply_NUM,
            .flags  = 0,
            .result = -EINVAL,
            .gsi    = 0,
        };

        send_message_port(d->reply_port, sizeof(reply), (char *)&reply);
        return;
    }

    struct PCIHostBridge *g = pci_host_bridge_find(d->group);
    if (!g) {
        IPC_Request_PCI_Device_GSI_Reply reply = {
            .type   = IPC_Request_PCI_Device_GSI_Reply_NUM,
            .flags  = 0,
            .result = -ENOENT,
            .gsi    = 0,
        };

        send_message_port(d->reply_port, sizeof(reply), (char *)&reply);
        return;
    }

    uint32_t gsi    = 0;
    bool active_low = false;
    bool level_trig = false;
    int ret         = resolve_gsi_for(g, d->bus, d->device, d->function, d->pin, &gsi, &active_low,
                                      &level_trig, NULL);
    IPC_Request_PCI_Device_GSI_Reply reply = {
        .type   = IPC_Request_PCI_Device_GSI_Reply_NUM,
        .flags  = 0,
        .result = ret,
        .gsi    = gsi,
    };

    send_message_port(d->reply_port, sizeof(reply), (char *)&reply);
}

int set_up_gsi(uint32_t gsi, bool active_low, bool level_trig, uint64_t task, pmos_port_t port,
               uint32_t *vector_out);

void register_pci_interrupt(Message_Descriptor *msg, IPC_Register_PCI_Interrupt *desc)
{
    IPC_Reg_Int_Reply reply;
    int result      = 0;
    uint32_t vector = 0;
    bool active_low = false;
    bool level_trig = false;

    if (msg->size != sizeof(IPC_Register_PCI_Interrupt)) {
        result = -EINVAL;
        goto end;
    }

    struct PCIHostBridge *g = pci_host_bridge_find(desc->group);
    if (!g) {
        result = -ENOENT;
        goto end;
    }

    result = resolve_gsi_for(g, desc->bus, desc->device, desc->function, desc->pin, &vector,
                             &active_low, &level_trig, NULL);
    if (result != 0)
        goto end;

    if (desc->dest_task == 0 || desc->dest_port == 0) {
        result = -EINVAL;
        goto end;
    }

    result = set_up_gsi(vector, active_low, level_trig, desc->dest_task, desc->dest_port, &vector);
end:
    reply = (IPC_Reg_Int_Reply) {
        .type   = IPC_Reg_Int_Reply_NUM,
        .flags  = 0,
        .status = result,
        .intno  = vector,
    };

    result = send_message_port(desc->reply_port, sizeof(reply), (char *)&reply);
    if (result != 0)
        printf("Failed to send message in register_pci_interrupt: %i (%s)\n", result,
               strerror(-result));
}

#ifdef PLATFORM_HAS_IO
static pthread_mutex_t pci_access_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

uint32_t pci_read_register(struct PCIDevicePtr *s, unsigned _register)
{
    assert(s);

    switch (s->type) {
    case PCIGroupECAM:
        return pci_mmio_readl(s->ecam_window + _register);
#ifdef PLATFORM_HAS_IO
    case PCIGroupLegacy:
        if (_register >= 256 / 4)
            return -1U;

        pthread_mutex_lock(&pci_access_mutex);
        io_out32(s->io_addr.port, (uint32_t)0x80000000 | s->io_addr.data_offset | (_register << 2));
        uint32_t data = io_in32(s->io_addr.port + 4);
        pthread_mutex_unlock(&pci_access_mutex);
        return data;
#endif
    default:
        assert(false);
    }
    return -1U;
}

void pci_write_register(struct PCIDevicePtr *s, unsigned _register, uint32_t value)
{
    assert(s);

    switch (s->type) {
    case PCIGroupECAM:
        return pci_mmio_writel(s->ecam_window + _register, value);
#ifdef PLATFORM_HAS_IO
    case PCIGroupLegacy:
        if (_register >= 256 / 4)
            return;

        pthread_mutex_lock(&pci_access_mutex);
        io_out32(s->io_addr.port, (uint32_t)0x80000000 | s->io_addr.data_offset | (_register << 2));
        io_out32(s->io_addr.port + 4, value);
        pthread_mutex_unlock(&pci_access_mutex);
        break;
#endif
    default:
        assert(false);
    }
}

int fill_device(struct PCIDevicePtr *s, struct PCIHostBridge *g, uint8_t bus, uint8_t device, uint8_t function)
{
    assert(s);
    assert(g);
    if (g->has_ecam) {
        void *config_space_virt    = g->ecam.base_ptr;
        const unsigned long offset = (bus << 20) | (device << 15) | (function << 12);

        *s = (struct PCIDevicePtr) {
            .type        = PCIGroupECAM,
            .ecam_window = (uint32_t *)((char *)config_space_virt + offset),
        };
    } else if (g->has_io) {
        if ((bus < g->start_bus_number) || (bus > g->end_bus_number))
            return -1;

        *s = (struct PCIDevicePtr) {
            .type = PCIGroupLegacy,
            .io_addr =
                (struct LegacyAddr) {
                    .data_offset = (bus << 16) | (device << 11) | (function << 8),
                    .port        = g->legacy.config_addr_io,
                },
        };

        return 0;
    } else {
        return -1;
    }

    return 0;
}

int fill_device_early(struct PCIDevicePtr *s, uint8_t bus, uint8_t device, uint8_t function)
{
    *s = (struct PCIDevicePtr) {
        .type = PCIGroupLegacy,
        .io_addr =
            (struct LegacyAddr) {
                .data_offset = (bus << 16) | (device << 11) | (function << 8),
                .port        = 0x3f8,
            },
    };

    return 0;
}