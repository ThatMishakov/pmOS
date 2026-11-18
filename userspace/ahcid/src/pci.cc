#include "pci.hh"

#include <errno.h>
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

pmos::ReceiveRight send_devicesd(auto &request, pmos::Port *reply_port)
{
    auto r =
        send_message_right_one(device_right, request, {reply_port, pmos::RightType::SendOnce});
    if (!r) {
        // TODO: Eventually don't throw
        printf("Failed to send message to devicesd: %i (%s)\n", (int)r.error().first,
               strerror(r.error().first));
        throw std::system_error(r.error().first, std::generic_category(),
                                "Failed to send message to devicesd");
    }
    return std::move(*r);
}

pmos::ReceiveRight send_devicesd(std::span<const uint8_t> data, pmos::Port *reply_port)
{
    auto r = send_message_right(device_right, data, {reply_port, pmos::RightType::SendOnce});
    if (!r) {
        printf("Failed to send message to devicesd: %i (%s)\n", (int)r.error().first,
               strerror(r.error().first));
        throw std::system_error(r.error().first, std::generic_category(),
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

pmos::async::task<pmos::ReceiveRight> PCIDevice::register_interrupt()
{
    IPC_Request_Interrupt request = {
        .type       = IPC_Request_Interrupt_NUM,
        .flags      = 0,
        .index      = 0,
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

pmos::async::task<PCIDevice::DeviceType> PCIDevice::get_device_type()
{
    IPC_Get_Device_Type request = {
        .type  = IPC_Get_Device_Type_NUM,
        .flags = 0,
    };

    auto right = send_devicesd(request, &cmd_port);
    auto msg = co_await dispatcher.get_message(right);

    if (msg->data.size() < sizeof(IPC_Get_Device_Type_Reply))
        throw std::system_error(EPROTO, std::generic_category(), "Unexpected message size in get_device_type");

    auto *reply = (IPC_Get_Device_Type_Reply *)msg->data.data();
    if (reply->type != IPC_Get_Device_Type_Reply_NUM)
        throw std::system_error(EPROTO, std::generic_category(), "Unexpected message type in get_device_type");

    switch (reply->device_type) {
    case IPC_DEVICE_TYPE_PCI:
        co_return DeviceType::PCI;
    case IPC_DEVICE_TYPE_ACPI:
        co_return DeviceType::ACPI;
    default:
        co_return DeviceType::Unknown;
    };
}

pmos::async::task<PCIDevice::Resource> PCIDevice::get_resource(RequestType request_type, uint32_t index, std::string_view name, ResourceType type)
{
    std::unique_ptr<uint8_t[]> msg = std::make_unique<uint8_t[]>(sizeof(IPC_Request_Resource) + name.size());
    IPC_Request_Resource *request = (IPC_Request_Resource *)msg.get();
    request->type = IPC_Request_Resource_NUM;
    request->flags = 0;
    switch (request_type.index()) {
    case 0:
        request->lookup_type = LOOKUP_TYPE_PCI_BAR;
        break;
    case 1:
        request->lookup_type = LOOKUP_TYPE_DEFAULT;
        break;
    default:
        throw std::system_error(EINVAL, std::generic_category(), "Invalid request type");
    }

    switch (type) {
    case ResourceType::Memory:
        request->resource_type = IPC_DEVICE_RESOURCE_MMIO;
        break;
    case ResourceType::IO:
        request->resource_type = IPC_DEVICE_RESOURCE_IO_PORT;
        break;
    case ResourceType::Any:
        request->resource_type = IPC_DEVICE_RESOURCE_ANY;
        break;
    }

    memcpy(request->resource_name, name.data(), name.size());
    request->resource_index = index;

    auto right = send_devicesd(std::span<const uint8_t>(msg.get(), sizeof(IPC_Request_Resource) + name.size()), &cmd_port);
    auto msg_reply = co_await dispatcher.get_message(right);

    if (msg_reply->data.size() < sizeof(IPC_Request_Resource_Reply))
        throw std::system_error(EPROTO, std::generic_category(), "Unexpected message size in get_resource");

    auto *reply = (IPC_Request_Resource_Reply *)msg_reply->data.data();
    if (reply->type != IPC_Request_Resource_Reply_NUM)
        throw std::system_error(EPROTO, std::generic_category(), "Unexpected message type in get_resource");

    if (reply->result < 0)
        throw std::system_error(-reply->result, std::generic_category(), "Failed to get resource");

    PCIDevice::Resource resource{};
    switch (reply->resource_type) {
    case IPC_DEVICE_RESOURCE_MMIO:
        resource.type = ResourceType::Memory;
        break;
    case IPC_DEVICE_RESOURCE_IO_PORT:
        resource.type = ResourceType::IO;
        break;
    default:
        resource.type = ResourceType::Unknown;
        break;
    }
    resource.base = reply->resource_start;
    resource.size = reply->resource_length;
    co_return resource;
}