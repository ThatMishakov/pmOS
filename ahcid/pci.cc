#include "pci.hh"

#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include <pmos/memory.h>
#include <pmos/ports.h>
#include <string.h>
#include <sys/mman.h>
#include <system_error>
#include <errno.h>
#include <stdlib.h>

extern pmos_port_t devicesd_port;
extern pmos_port_t cmd_port;

// TODO: pthread stuff

void send_devicesd(auto &request)
{
    auto r = send_message_port(devicesd_port, sizeof(request), (void *)&request);
    if (r != 0) {
        // TODO: Eventually don't throw
        printf("Failed to send message to devicesd: %li (%s)\n", r, strerror(-r));
        throw std::system_error(r, std::generic_category(), "Failed to send message to devicesd");
    }
}

PCIDevice::PCIDevice(uint16_t group, uint8_t bus, uint8_t device, uint8_t function)
{
    this->group    = group;
    this->bus      = bus;
    this->device   = device;
    this->function = function;

    IPC_Request_PCI_Device req = {
        .type       = IPC_Request_PCI_Device_NUM,
        .flags      = 0,
        .reply_port = cmd_port,
        .group      = group,
        .bus        = bus,
        .device     = device,
        .function   = function,
        .reserved   = 0,
    };

    send_devicesd(req);

    Message_Descriptor desc;
    uint8_t *message;
    result_t result = get_message(&desc, &message, cmd_port);
    if (result != 0) {
        printf("Failed to get message\n");
        throw std::system_error(result, std::generic_category(), "Failed to get message");
    }

    auto *reply = (IPC_Request_PCI_Device_Reply *)message;
    if (reply->type != IPC_Request_PCI_Device_Reply_NUM) {
        printf("Unexpected message type\n");
        free(message);
        throw std::runtime_error("Unexpected message type");
    }

    if (reply->type_error < 0) {
        printf("Failed to get PCI device: %i (%s)\n", reply->type_error,
               strerror(-reply->type_error));
        free(message);
        throw std::system_error(reply->type_error, std::generic_category(),
                                "Failed to get PCI device");
    }

    if (reply->type_error != IPC_PCI_ACCESS_TYPE_MMIO) {
        printf("PCI device does not support MMIO\n");
        free(message);
        throw std::runtime_error("PCI device does not support MMIO");
    }

    auto base_phys = reply->base_address;
    free(message);

    // Map the PCI device's configuration space
    auto mem_req =
        create_phys_map_region(0, nullptr, 4096, PROT_READ | PROT_WRITE, base_phys);
    if (mem_req.result != SUCCESS)
        throw std::system_error(mem_req.result, std::generic_category(),
                                "Failed to map PCI device's configuration space");

    virt_addr = reinterpret_cast<char *>(mem_req.virt_addr);
}

PCIDevice::~PCIDevice()
{
    if (virt_addr) {
        munmap((void *)virt_addr, 4096);
        virt_addr = nullptr;
    }
}

uint8_t PCIDevice::readb(uint16_t offset) { return *((volatile uint8_t *)(virt_addr + offset)); }

uint16_t PCIDevice::readw(uint16_t offset) { return *((volatile uint16_t *)(virt_addr + offset)); }

uint32_t PCIDevice::readl(uint16_t offset) { return *((volatile uint32_t *)(virt_addr + offset)); }

void PCIDevice::writeb(uint16_t offset, uint8_t val)
{
    *((volatile uint8_t *)(virt_addr + offset)) = val;
}

void PCIDevice::writew(uint16_t offset, uint16_t val)
{
    *((volatile uint16_t *)(virt_addr + offset)) = val;
}

void PCIDevice::writel(uint16_t offset, uint32_t val)
{
    *((volatile uint32_t *)(virt_addr + offset)) = val;
}

char PCIDevice::interrupt_pin() noexcept
{
    return readb(0x3d);
}

int PCIDevice::gsi(uint32_t &gsi_result) noexcept
{
    auto pin = interrupt_pin();
    if (pin < 1 or pin > 4) {
        errno = ENOENT;
        return -1;
    }

    IPC_Request_PCI_Device_GSI request = {
        .type       = IPC_Request_PCI_Device_GSI_NUM,
        .flags      = 0,
        .reply_port = cmd_port,
        .group      = group,
        .bus        = bus,
        .device     = device,
        .function   = function,
        .pin        = (uint8_t)(pin - 1),
    };

    try {
        send_devicesd(request);
    } catch (std::system_error &e) {
        errno = e.code().value();
        return -1;
    }

    Message_Descriptor desc;
    uint8_t *message;

    result_t result = get_message(&desc, &message, cmd_port);
    if (result != 0) {
        errno = result;
        return -1;
    }

    auto *reply = (IPC_Request_PCI_Device_GSI_Reply *)message;
    if (reply->type != IPC_Request_PCI_Device_GSI_Reply_NUM) {
        free(message);
        errno = EPROTO;
        return -1;
    }

    if (reply->result < 0) {
        errno = -reply->result;
        free(message);
        return -1;
    }

    gsi_result = reply->gsi;
    free(message);

    return 0;
}

int PCIDevice::register_interrupt(uint32_t &int_vector_result, uint64_t task, uint64_t port) noexcept
{
    auto int_pin = interrupt_pin();
    if (int_pin < 1 or int_pin > 4) {
        return -ENOENT;
    }

    IPC_Register_PCI_Interrupt request = {
        .type       = IPC_Register_PCI_Interrupt_NUM,
        .flags      = 0,
        .group      = group,
        .bus        = bus,
        .device     = device,
        .function   = function,
        .pin        = (uint8_t)(int_pin - 1),
        .dest_task       = task == 0 ? get_task_id() : task,
        .dest_port       = port,
        .reply_port      = cmd_port,
    };

    try {
        send_devicesd(request);
    } catch (std::system_error &e) {
        return -e.code().value();
    }

    Message_Descriptor desc;
    uint8_t *message;
    for (int i = 0; i < 5; ++i) {
        result_t result = get_message(&desc, &message, cmd_port);
        // The affinity may be changed, which can interrupt the system call
        // This is not an error, so retry a few times
        if ((int)result == -EINTR)
            continue;

        if (result != 0) {
            return result;
        }
        break;
    }

    auto *reply = (IPC_Reg_Int_Reply *)message;
    if (reply->type != IPC_Reg_Int_Reply_NUM) {
        free(message);
        return -EPROTO;
    }

    if (reply->status < 0) {
        int err = reply->status;
        free(message);
        return err;
    }

    int_vector_result = reply->intno;
    free(message);
    return 0;
}