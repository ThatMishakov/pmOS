#pragma once
#include <cstdint>
#include <pmos/io.h>

extern uint32_t *nvme_virt_base;

extern uint32_t doorbell_stride; // register * (1 << doorbell_stride)
extern uint32_t timeout; // 500 milliseconds units
extern uint32_t maximum_queue_entries;

constexpr uint32_t NVME_REG_CAP = 0x0;
constexpr uint32_t NVME_REG_VS  = 0x8;
constexpr uint32_t NVME_REG_INTMS = 0xC;
constexpr uint32_t NVME_REG_INTMC = 0x10;
constexpr uint32_t NVME_REG_CC = 0x14;
constexpr uint32_t NVME_REG_CSTS = 0x1C;

constexpr uint32_t NVME_REG_AQA = 0x24;
constexpr uint32_t NVME_REG_ASQ = 0x28;
constexpr uint32_t NVME_REG_ACQ = 0x30;

constexpr uint32_t NVME_REG_SQ0TDBL = 0x1000;

inline uint32_t nvme_read32(uint32_t offset)
{
    return mmio_readl(nvme_virt_base + offset / 4);
}
inline uint64_t nvme_read64(uint32_t offset)
{
    auto low = mmio_readl(nvme_virt_base + offset / 4);
    auto high = mmio_readl(nvme_virt_base + offset / 4 + 1);
    return (uint64_t)high << 32 | low;
}
inline void nvme_write32(uint32_t offset, uint32_t value)
{
    mmio_writel(nvme_virt_base + offset / 4, value);
}
inline void nvme_write64(uint32_t offset, uint64_t value)
{
    mmio_writel(nvme_virt_base + offset / 4, value & 0xffffffff);
    mmio_writel(nvme_virt_base + offset / 4 + 1, value >> 32);
}

inline uint16_t dma_read16(const uint16_t &addr)
{
    return *(volatile uint16_t *)&addr;
}     