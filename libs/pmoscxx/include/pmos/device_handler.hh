#pragma once
#include <stdint.h>
#include <pmos/async/coroutines.hh>
#include <expected>
#include <memory>
#include <variant>
#include <pmos/helpers.hh>
#include <string>
#include "helpers.hh"

namespace pmos
{

class DeviceHandler
{
public:
    // // Resolves the device's interrupt line to GSI
    // // Returns 0 on success, otherwise -1 setting errno to the error code
    // int gsi(uint32_t &gsi_result) noexcept;

    // Registers the interrupt for the device
    // Returns 0 on success, otherwise -errno
    // This has to block, since it can change the affinity, and it might break the messaging (yet another TODO...)
    pmos::async::task<pmos::ReceiveRight> register_interrupt();

    enum class ResourceType {
        Memory,
        IO,
        Unknown,
        Any = Unknown,
    };

    struct PCIBar {};
    struct GenericResource {};

    using RequestType = std::variant<PCIBar, GenericResource>;

    struct Resource {
        ResourceType type;
        uint64_t base;
        uint64_t size;  
    };

    pmos::async::task<Resource> get_resource(RequestType request_type, uint32_t index, std::string_view name = {}, ResourceType type = ResourceType::Any);

    enum class DeviceType {
        Unknown,
        PCI,
        ACPI,
    };

    pmos::async::task<DeviceType> get_device_type();

    // PCI helper functions
    pmos::async::task<uint32_t> pci_readl(uint16_t offset);
    pmos::async::task<uint16_t> pci_readw(uint16_t offset);
    pmos::async::task<uint8_t> pci_readb(uint16_t offset);

    pmos::async::task<void> pci_writel(uint16_t offset, uint32_t val);
    pmos::async::task<void> pci_writew(uint16_t offset, uint16_t val);
    pmos::async::task<void> pci_writeb(uint16_t offset, uint8_t val);

    inline DeviceHandler(pmos::PortDispatcher &dispatcher, pmos::Right &device_right)
        : device_right(device_right), dispatcher(dispatcher)
    {
    }
private:
    pmos::Right &device_right;
    pmos::PortDispatcher &dispatcher;
};

} // namespace pmos
