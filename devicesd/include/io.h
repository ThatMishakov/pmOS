#ifndef IO_H
#define IO_H
#include <stdint.h>

#if defined(__x86_64__) || defined(__i386__) 
#define PLATFORM_HAS_IO 1

uint8_t io_in8(uint16_t port);
void io_out8(uint16_t port, uint8_t data);

uint16_t io_in16(uint16_t port);
void io_out16(uint16_t port, uint16_t data);

uint32_t io_in32(uint16_t port);
void io_out32(uint16_t port, uint32_t data);
#endif

#if defined(__x86_64__) || defined(__i386__)

inline uint32_t mmio_readl(uint32_t *ptr)
{
    uint32_t data;
    asm volatile ("movl %1, %k0" : "=r"(data) : "o"(*ptr) : "memory");
    return data;
}
inline void mmio_writel(uint32_t *ptr, uint32_t data)
{
    asm volatile("movl %k0, %1" :: "r"(data), "o"(*ptr) : "memory");
}

// AMD fun stuff: IO accesses must be through *ax register
inline uint32_t pci_mmio_readl(uint32_t *ptr)
{
    uint32_t data;
    asm volatile ("movl %1, %k0" : "=a"(data) : "o"(*ptr) : "memory");
    return data;
}

inline void pci_mmio_writel(uint32_t *ptr, uint32_t data)
{
    asm volatile("movl %k0, %1" :: "a"(data), "o"(*ptr) : "memory");
}
#endif

#if defined(__riscv)
inline uint32_t mmio_readl(uint32_t *ptr)
{
    uint32_t data;
    asm volatile ("lw %0, %1" : "=r"(data) : "o"(*ptr));
    return data;
}
inline void mmio_writel(uint32_t *ptr, uint32_t data)
{
    asm volatile("sw %0, %1" :: "r"(data), "o"(*ptr));
}

#define pci_mmio_readl mmio_readl
#define pci_mmio_writel mmio_writel
#endif

#endif