#include "pci.hh"

#include <alloca.h>
#include <memory>
#include <pmos/helpers.h>
#include <pmos/interrupts.h>
#include <pmos/ipc.h>
#include <pmos/memory.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

constexpr std::string devicesd_port_name = "/pmos/devicesd";
pmos_port_t devicesd_port                = []() -> auto {
    ports_request_t request =
        get_port_by_name(devicesd_port_name.c_str(), devicesd_port_name.length(), 0);
    return request.port;
}();

pmos_port_t _create_port()
{
    ports_request_t request = create_port(0, 0);
    if (request.port == 0) {
        printf("Failed to create port\n");
        return 0;
    }
    return request.port;
}

pmos_port_t cmd_port = _create_port();

struct PCIDescriptor {
    int group;
    int bus;
    int device;
    int function;
};

std::vector<PCIDescriptor> get_ahci_controllers()
{
    auto struct_size = sizeof(IPC_Request_PCI_Devices) + sizeof(IPC_PCIDevice) * 2;
    auto request     = (IPC_Request_PCI_Devices *)alloca(struct_size);

    *request = {
        .type       = IPC_Request_PCI_Devices_NUM,
        .flags      = 0,
        .reply_port = cmd_port,
    };
    request->devices[0] = {
        .vendor_id  = 0xffff,
        .device_id  = 0xffff,
        .class_code = 0x01,
        .subclass   = 0x06,
        .prog_if    = 0x01,
        .reserved   = 0,
    };
    request->devices[1] = {
        .vendor_id  = 0xffff,
        .device_id  = 0xffff,
        .class_code = 0x01,
        .subclass   = 0x04,
        .prog_if    = 0x00,
        .reserved   = 0,
    };

    result_t result = send_message_port(devicesd_port, struct_size, (unsigned char *)request);
    if (result != 0) {
        printf("Failed to request PCI devices\n");
        return {};
    }

    Message_Descriptor desc;
    uint8_t *message;
    result = get_message(&desc, &message, cmd_port);
    if (result != 0) {
        printf("Failed to get message\n");
        return {};
    }

    auto reply = (IPC_Request_PCI_Devices_Reply *)message;
    if (reply->type != IPC_Request_PCI_Devices_NUM) {
        printf("Invalid reply type\n");
        return {};
    }

    if (reply->result_num_of_devices < 0) {
        printf("Failed to get PCI devices: %li (%s)\n", reply->result_num_of_devices,
               strerror(-reply->result_num_of_devices));
        return {};
    }

    std::vector<PCIDescriptor> controllers;
    for (int i = 0; i < reply->result_num_of_devices; ++i) {
        auto &device = reply->devices[i];
        controllers.push_back({
            .group    = device.group,
            .bus      = device.bus,
            .device   = device.device,
            .function = device.function,
        });
    }

    return controllers;
}

std::unique_ptr<PCIDevice> ahci_controller = nullptr;
volatile uint32_t *ahci_virt_base          = nullptr;

static constexpr uint32_t AHCI_GHC_HR   = 1 << 0;
static constexpr uint32_t AHCI_GHC_IE   = 1 << 1;
static constexpr uint32_t AHCI_GHC_MRSM = 1 << 2;
static constexpr uint32_t AHCI_GHC_AE   = 1 << 31;

static constexpr uint32_t AHCI_CAP2_BOH = 1 << 0;

static constexpr uint32_t AHCI_BOHC_BOS  = 1 << 0;
static constexpr uint32_t AHCI_BOHC_OOS  = 1 << 1;
static constexpr uint32_t AHCI_BOHC_SOOE = 1 << 2;
static constexpr uint32_t AHCI_BOHC_OOC  = 1 << 3;
static constexpr uint32_t AHCI_BOHC_BB   = 1 << 4;

static constexpr uint32_t SATA_SIG_ATA   = 0x00000101;
static constexpr uint32_t SATA_SIG_ATAPI = 0xEB140101;
static constexpr uint32_t SATA_SIG_PM    = 0x96690101;
static constexpr uint32_t SATA_SIG_SEMB  = 0xC33C0101;

static constexpr uint32_t AHCI_MAX_PORTS = 32;

void reset_controller()
{
    printf("Resetting AHCI controller\n");

    // 10.4.3 of AHCI spec

    uint32_t ghc = ahci_virt_base[1];
    ghc |= AHCI_GHC_HR;
    ahci_virt_base[1] = ghc;

    // Give controller 1s to clear HR
    for (int i = 0; i < 10; ++i) {
        usleep(100000);
        if (!(ahci_virt_base[1] & AHCI_GHC_HR))
            break;
    }

    if (ahci_virt_base[1] & AHCI_GHC_HR) {
        printf("Failed to reset AHCI controller... Exiting\n");
        exit(1);
    }

    printf("AHCI controller reset\n");
}

int num_slots       = 0;
int interface_speed = 0;

static constexpr uint32_t FIS_RECIEVE_AREA_SIZE = 0x1000;

static constexpr uint32_t PRDT_OFFSET           = 0x80;
static constexpr uint32_t PRDT_SIZE_BYTES       = 0x10;
static constexpr uint32_t COMMAND_TABLE_ENTRIES = 8;
static constexpr uint32_t COMMAND_TABLE_SIZE    = // 0x100
    PRDT_OFFSET + PRDT_SIZE_BYTES * COMMAND_TABLE_ENTRIES;

struct CommandListStructure {
    // 4.2.2 of AHCI spec
    uint32_t descriptor_information;
    uint32_t PRDBC; // Physical Region Descriptor Byte Count
    uint32_t command_table_base_low;
    uint32_t command_table_base_high;

    uint32_t reserved[4];
};

struct AHCIPort {
    int index;
    uint64_t dma_phys_base           = -1;
    volatile uint32_t *dma_virt_base = nullptr;

    static constexpr uint32_t recieved_fis_offset = 0x0;
    // Put it here for FIS-based switching
    static constexpr uint32_t command_list_offset = 0x1000;

    // 1 command table per command list, with 8 entries max
    static constexpr uint32_t command_table_offset = 0x1000 + 0x400;

    volatile uint32_t *get_port_register();
};

std::vector<AHCIPort> ports;

void ahci_handle(PCIDescriptor d)
{
    ahci_controller = std::make_unique<PCIDevice>(d.group, d.bus, d.device, d.function);

    printf("AHCI controller created\n");

    uint16_t vendor_id = ahci_controller->readw(0);
    uint16_t device_id = ahci_controller->readw(2);
    uint8_t class_code = ahci_controller->readb(0xb);
    uint8_t subclass   = ahci_controller->readb(0xa);
    uint8_t prog_if    = ahci_controller->readb(0x9);
    printf("AHCI controller: vendor 0x%x device 0x%x class 0x%x subclass 0x%x prog_if 0x%x\n",
           vendor_id, device_id, class_code, subclass, prog_if);

    // Enable DMA and memory space access
    uint32_t command = ahci_controller->readb(0x4);
    command |= 0x06;
    ahci_controller->writeb(0x4, command);

    // Get BAR5
    uint32_t bar5 = ahci_controller->readl(0x24);
    printf("ACHI BAR5: 0x%x\n", bar5);

    bar5 &= 0xfffffff0;

    auto mem_req =
        create_phys_map_region(0, nullptr, 8192, PROT_READ | PROT_WRITE, (void *)(size_t)bar5);
    if (mem_req.result != SUCCESS) {
        printf("Failed to map AHCI controller's memory\n");
        return;
    }
    ahci_virt_base = reinterpret_cast<volatile uint32_t *>(mem_req.virt_addr);

    uint32_t ghc = ahci_virt_base[1];
    // Enable AHCI
    ghc |= 0x80000000;
    ahci_virt_base[1] = ghc;

    uint32_t cap = ahci_virt_base[0];
    printf("AHCI Capabilities: 0x%x\n", cap);

    bool s64a = !!(cap & (1 << 31));
    if (!s64a) {
        printf("AHCI controller does not support 64-bit addressing\n");
        return;
    }

    uint32_t cap2 = ahci_virt_base[9];
    printf("AHCI Capabilities 2: 0x%x\n", cap2);

    uint32_t ahci_version = ahci_virt_base[4];
    printf("AHCI Version: 0x%x\n", ahci_version);

    // BIOS handoff
    if (cap2 & AHCI_CAP2_BOH) {
        // 10.6.3 of AHCI spec
        printf("BIOS handoff supported... performing it\n");

        // Set OS Ownership to 1
        uint32_t bohc      = ahci_virt_base[10];
        ahci_virt_base[10] = bohc | AHCI_BOHC_BOS;

        // Wait for 25ms for BOS to be cleared
        for (int i = 0; i < 25; ++i) {
            if (!(ahci_virt_base[10] & AHCI_BOHC_BOS))
                break;
            usleep(1000);
        }

        if (!(ahci_virt_base[10] & AHCI_BOHC_BOS)) {
            printf("BIOS handoff successful\n");
        } else if (ahci_virt_base[10] & AHCI_BOHC_BB) {
            printf("BIOS busy... waiting for it to finish\n");

            // Sleep for 2s in 100ms intervals
            for (int i = 0; i < 20; ++i) {
                usleep(100000);
                if (!(ahci_virt_base[10] & AHCI_BOHC_BB)) {
                    printf("BIOS handoff successful\n");
                    break;
                }
            }
            printf("BIOS timed out...\n");
        } else {
            printf("BIOS timed out...\n");
        }
    }

    // Reset the controller
    reset_controller();

    // Enable ACPI again
    ghc = ahci_virt_base[1];
    ghc |= AHCI_GHC_IE;
    ahci_virt_base[1] = ghc;

    cap = ahci_virt_base[0];

    num_slots       = (cap >> 8) & 0x1f;
    interface_speed = (cap >> 20) & 0xf;

    printf("AHCI controller initialized\n");

    // TODO: Initialize ports
    uint32_t pi = ahci_virt_base[4];
    printf("AHCI Ports Implemented: 0x%x\n", pi);

    for (unsigned i = 0; i < AHCI_MAX_PORTS; ++i) {
        if (pi & (1 << i)) {
            printf("Port %i implemented\n", i);
            ports.push_back({.index = (int)i});
        }
    }

    for (auto &port: ports) {
        size_t mem_size = port.command_table_offset + num_slots * COMMAND_TABLE_SIZE;
        // TODO: Align to page size constants
        mem_size        = (mem_size + 0xfff) & ~0xfff;

        auto request =
            create_normal_region(0, nullptr, mem_size, PROT_READ | PROT_WRITE | CREATE_FLAG_DMA);
        if ((long)request.result < 0) {
            printf("Failed to allocate memory for AHCI: %li (%s)\n", request.result,
                   strerror(-request.result));
            exit(1);
        }

        auto phys_addr = get_page_phys_address(0, request.virt_addr, 0);
        if ((long)phys_addr.result < 0) {
            printf("Could not get physical address for AHCI: %li (%s)\n", phys_addr.result,
                   strerror(-request.result));
            exit(1);
        }

        port.dma_phys_base = phys_addr.phys_addr;
        port.dma_virt_base = reinterpret_cast<volatile uint32_t*>(request.virt_addr);

        printf("Allocated memory for port %i: 0x%lx - 0x%lx -> 0x%lx\n", port.index, port.dma_phys_base, port.dma_phys_base + mem_size, port.dma_virt_base);

        for (int i = 0; i < num_slots; ++i) {
            auto &command_list = reinterpret_cast<volatile CommandListStructure *>(
                port.dma_virt_base + port.command_list_offset / sizeof(uint32_t))[i];
            command_list.descriptor_information = 0x80000000;
            command_list.PRDBC                  = 0;
            uint64_t command_table_phys_base =
                port.dma_phys_base + port.command_table_offset + i * COMMAND_TABLE_SIZE;
            command_list.command_table_base_low  = command_table_phys_base & 0xffffffff;
            command_list.command_table_base_high = command_table_phys_base >> 32;
        }
    }
}

int main()
{
    printf("Hello from AHCId! My PID: %li\n", getpid());

    auto controllers = get_ahci_controllers();
    if (controllers.empty()) {
        printf("No AHCI controllers found\n");
        return 0;
    }

    for (const auto &controller: controllers) {
        printf("Found AHCI controller: group %i bus %i device %i function %i\n", controller.group,
               controller.bus, controller.device, controller.function);

        auto r = fork();
        if (r == 0) {
            cmd_port = _create_port();
            ahci_handle(controller);
            return 0;
        } else if (r < 0) {
            printf("Failed to fork\n");
        }
    }
}