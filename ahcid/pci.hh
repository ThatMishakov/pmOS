#pragma once
#include <stdint.h>

class PCIDevice
{
public:
    uint32_t readl(uint16_t offset);
    uint16_t readw(uint16_t offset);
    uint8_t readb(uint16_t offset);

    void writel(uint16_t offset, uint32_t val);
    void writew(uint16_t offset, uint16_t val);
    void writeb(uint16_t offset, uint8_t val);

    PCIDevice(uint16_t group, uint8_t bus, uint8_t device, uint8_t function);
    ~PCIDevice();

    PCIDevice(const PCIDevice &other)            = delete;
    PCIDevice &operator=(const PCIDevice &other) = delete;

    PCIDevice(PCIDevice &&other);
    PCIDevice &operator=(PCIDevice &&other);

    // Returns 0 if no interrupt pin is connected, otherwise 0x1 for INTA# to 0x4 for INTD#
    char interrupt_pin() noexcept;

    // Resolves the device's interrupt line to GSI
    // Returns 0 on success, otherwise -1 setting errno to the error code
    int gsi(uint32_t &gsi_result) noexcept;

    // Registers the interrupt for the device
    // Returns 0 on success, otherwise -errno
    int register_interrupt(uint32_t &int_vector_result, uint64_t task, uint64_t port) noexcept;
private:
    PCIDevice();

    volatile char *virt_addr;
    uint16_t group;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
};