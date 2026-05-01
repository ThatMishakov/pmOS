#pragma once
#include <stdint.h>
#include <pmos/async/coroutines.hh>
#include <expected>
#include <memory>
#include <pmos/helpers.hh>

struct PCIDevice
{
    pmos::async::task<uint32_t> readl(uint16_t offset);
    pmos::async::task<uint16_t> readw(uint16_t offset);
    pmos::async::task<uint8_t> readb(uint16_t offset);

    pmos::async::task<void> writel(uint16_t offset, uint32_t val);
    pmos::async::task<void> writew(uint16_t offset, uint16_t val);
    pmos::async::task<void> writeb(uint16_t offset, uint8_t val);

    PCIDevice(const PCIDevice &other)            = delete;
    PCIDevice &operator=(const PCIDevice &other) = delete;

    PCIDevice(PCIDevice &&other);
    PCIDevice &operator=(PCIDevice &&other);

    PCIDevice(volatile char *virt_addr);

    // Returns 0 if no interrupt pin is connected, otherwise 0x1 for INTA# to 0x4 for INTD#
    pmos::async::task<uint8_t> interrupt_pin() noexcept;

    // // Resolves the device's interrupt line to GSI
    // // Returns 0 on success, otherwise -1 setting errno to the error code
    // int gsi(uint32_t &gsi_result) noexcept;

    // Registers the interrupt for the device
    // Returns 0 on success, otherwise -errno
    // This has to block, since it can change the affinity, and it might break the messaging (yet another TODO...)
    pmos::async::task<pmos::RecieveRight> register_interrupt();

    uint16_t group() const;
    uint8_t bus() const;
    uint8_t device() const;
    uint8_t function() const;

    PCIDevice() = default;
};

pmos::async::task<std::unique_ptr<PCIDevice>> get_pci_device();