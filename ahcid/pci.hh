#pragma once
#include <stdint.h>

class PCIDevice {
public:
    uint32_t readl(uint16_t offset);
    uint16_t readw(uint16_t offset);
    uint8_t readb(uint16_t offset);

    void writel(uint16_t offset, uint32_t val);
    void writew(uint16_t offset, uint16_t val);
    void writeb(uint16_t offset, uint8_t val);

    PCIDevice(uint16_t group, uint8_t bus, uint8_t device, uint8_t function);
    ~PCIDevice();

    PCIDevice(const PCIDevice &other) = delete;
    PCIDevice &operator=(const PCIDevice &other) = delete;

    PCIDevice(PCIDevice &&other);
    PCIDevice &operator=(PCIDevice &&other);
private:
    PCIDevice();

    volatile char *virt_addr;
    uint16_t group;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
};