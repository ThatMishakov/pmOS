#include "pci.hh"

#include <errno.h>
#include <pmos/helpers.h>
#include <pmos/helpers.hh>
#include <pmos/ipc.h>
#include <pmos/memory.h>
#include <pmos/ports.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <system_error>
#include <inttypes.h>

extern pmos::Right device_right;
extern pmos::Port cmd_port;
extern pmos::PortDispatcher dispatcher;

// TODO: pthread stuff

pmos::RecieveRight send_devicesd(auto &request, pmos::Port *reply_port)
{
    auto r =
        send_message_right_one(device_right, request, {reply_port, pmos::RightType::SendOnce});
    if (!r) {
        // TODO: Eventually don't throw
        printf("Failed to send message to devicesd: %i (%s)\n", (int)r.error(),
               strerror(r.error()));
        throw std::system_error(r.error(), std::generic_category(),
                                "Failed to send message to devicesd");
    }
    return std::move(*r);
}

pmos::async::task<std::unique_ptr<PCIDevice>> get_pci_device()
{
    IPC_Request_PCI_Device req = {
        .type       = IPC_Request_PCI_Device_NUM,
        .flags      = 0,
    };

    auto right = send_devicesd(req, &cmd_port);
    auto msg = co_await dispatcher.get_message(right);

    if (!msg) {
        printf("Failed to get message from devicesd\n");
        throw std::runtime_error("Failed to get message\n");
    }

    if (msg->data.size() < sizeof(IPC_Request_PCI_Device_Reply)) {
        printf("Unexpected message size\n");
        throw std::runtime_error("Unexpected message size");
    }

    auto *reply = (IPC_Request_PCI_Device_Reply *)msg->data.data();
    if (reply->type != IPC_Request_PCI_Device_Reply_NUM) {
        printf("Unexpected message type %" PRIu32 "\n", reply->type);
        throw std::runtime_error("Unexpected message type");
    }

    if (reply->type_error < 0) {
        printf("Failed to get PCI device: %i (%s)\n", reply->type_error,
               strerror(-reply->type_error));
        throw std::system_error(reply->type_error, std::generic_category(),
                                "Failed to get PCI device");
    }

    if (reply->type_error != IPC_PCI_ACCESS_TYPE_MMIO) {
        printf("PCI device does not support MMIO\n");
        throw std::runtime_error("PCI device does not support MMIO");
    }

    auto base_phys = reply->base_address;

    // Map the PCI device's configuration space
    auto mem_req = create_phys_map_region(0, nullptr, 4096, PROT_READ | PROT_WRITE, base_phys);
    if (mem_req.result != SUCCESS)
        throw std::system_error(mem_req.result, std::generic_category(),
                                "Failed to map PCI device's configuration space");

    co_return std::make_unique<PCIDevice>(reinterpret_cast<char *>(mem_req.virt_addr));
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

char PCIDevice::interrupt_pin() noexcept { return readb(0x3d); }

// int PCIDevice::gsi(uint32_t &gsi_result) noexcept
// {
//     auto pin = interrupt_pin();
//     if (pin < 1 or pin > 4) {
//         errno = ENOENT;
//         return -1;
//     }

//     IPC_Request_PCI_Device_GSI request = {
//         .type       = IPC_Request_PCI_Device_GSI_NUM,
//         .flags      = 0,
//         .pin        = (uint8_t)(pin - 1),
//     };

//     try {
//         send_devicesd(request, &cmd_port);
//     } catch (std::system_error &e) {
//         errno = e.code().value();
//         return -1;
//     }

//     Message_Descriptor desc;
//     uint8_t *message;

//     result_t result = get_message(&desc, &message, cmd_port.get(), nullptr, nullptr);
//     if (result != 0) {
//         errno = result;
//         return -1;
//     }

//     auto *reply = (IPC_Request_PCI_Device_GSI_Reply *)message;
//     if (reply->type != IPC_Request_PCI_Device_GSI_Reply_NUM) {
//         free(message);
//         errno = EPROTO;
//         return -1;
//     }

//     if (reply->result < 0) {
//         errno = -reply->result;
//         free(message);
//         return -1;
//     }

//     gsi_result = reply->gsi;
//     free(message);

//     return 0;
// }

pmos::async::task<pmos::RecieveRight> PCIDevice::register_interrupt()
{
    auto int_pin = interrupt_pin();
    if (int_pin < 1 or int_pin > 4)
        throw std::runtime_error("Device does not have a valid interrupt pin");

    IPC_Request_PCI_Interrupt request = {
        .type       = IPC_Request_PCI_Interrupt_NUM,
        .flags      = 0,
        .pin        = (uint8_t)(int_pin - 1),
    };

    auto right = send_devicesd(request, &cmd_port);
    auto msg = co_await dispatcher.get_message(right);

    if (msg->data.size() < sizeof(IPC_Request_Int_Reply))
        throw std::system_error(EPROTO, std::generic_category(), "Unexpected message size in register_interrupt");

    auto *reply = (IPC_Request_Int_Reply *)msg->data.data();
    if (reply->type != IPC_Request_Int_Reply_NUM)
        throw std::system_error(EPROTO, std::generic_category(), "Unexpected message type in register_interrupt");

    if (reply->status < 0)
        throw std::system_error(-reply->status, std::generic_category(), "Failed to register interrupt");

    auto int_right = std::move(msg->other_rights[0]);
    if (!int_right)
        throw std::system_error(EPROTO, std::generic_category(), "Expected interrupt source right in register_interrupt");

    if (int_right.type() != pmos::RightType::IntSource)
        throw std::system_error(EPROTO, std::generic_category(), "Expected interrupt source right in register_interrupt");

    co_return pmos::register_interrupt(int_right, cmd_port);
}

PCIDevice::PCIDevice(volatile char *virt_addr): virt_addr(virt_addr) {}