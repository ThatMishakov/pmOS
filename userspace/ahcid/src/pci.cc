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
    co_return std::make_unique<PCIDevice>();
}

pmos::async::task<void> PCIDevice::writew(uint16_t offset, uint16_t val)
{
    if (offset & 1)
        throw std::system_error(EINVAL, std::system_category(), "Misaligned access");

    IPC_PCI_Write req = {
        .type = IPC_PCI_Write_NUM,
        .offset = offset,
        .access_width = 2,
        .data = val,
    };

    auto right = send_devicesd(req, &cmd_port);
    auto msg = co_await dispatcher.get_message(right);

    if (msg->data.size() < sizeof(IPC_PCI_Write_Result))
        throw std::system_error(EPROTO, std::system_category(), "Unexpected message size in PCIDevice::writew");

    auto *reply = reinterpret_cast<IPC_PCI_Write_Result *>(msg->data.data());
    if (reply->type != IPC_PCI_Write_Result_NUM)
        throw std::system_error(EPROTO, std::system_category(), "Unexpected message type in PCIDevice::writew");

    if (reply->result)
        throw std::system_error(-reply->result, std::system_category());

    co_return;
}

pmos::async::task<uint32_t> PCIDevice::readl(uint16_t offset) {
    if (offset & 3)
        throw std::system_error(EINVAL, std::system_category(), "Misaligned access");

    IPC_PCI_Read req = {
        .type = IPC_PCI_Read_NUM,
        .offset = offset,
        .access_width = 4,
    };

    auto right = send_devicesd(req, &cmd_port);
    auto msg = co_await dispatcher.get_message(right);

    if (msg->data.size() < sizeof(IPC_PCI_Read_Result))
        throw std::system_error(EPROTO, std::system_category(), "Unexpected message size in PCIDevice::readl");

    auto *reply = reinterpret_cast<IPC_PCI_Read_Result *>(msg->data.data());
    if (reply->type != IPC_PCI_Read_Result_NUM)
        throw std::system_error(EPROTO, std::system_category(), "Unexpected message type in PCIDevice::readl");

    if (reply->result)
        throw std::system_error(-reply->result, std::system_category());

    co_return reply->data;
}

pmos::async::task<uint16_t> PCIDevice::readw(uint16_t offset)
{
    if (offset & 1)
        throw std::system_error(EINVAL, std::system_category(), "Misaligned access");

    auto result = co_await readl(offset & ~3);
    if (offset & 2)
        co_return result >> 16;
    else
        co_return result;
}
pmos::async::task<uint8_t> PCIDevice::readb(uint16_t offset)
{
    auto result = co_await readl(offset & ~3);
    co_return result >> (offset & 3) * 8;
}

pmos::async::task<uint8_t> PCIDevice::interrupt_pin() noexcept { return readb(0x3d); }

pmos::async::task<pmos::RecieveRight> PCIDevice::register_interrupt()
{
    auto int_pin = co_await interrupt_pin();
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