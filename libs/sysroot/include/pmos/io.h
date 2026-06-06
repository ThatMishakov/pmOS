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
inline uint16_t mmio_readh(uint16_t *ptr)
{
    uint16_t data;
    asm volatile ("movw %1, %w0" : "=r"(data) : "o"(*ptr) : "memory");
    return data;
}
inline uint8_t mmio_readb(uint8_t *ptr)
{
    uint8_t data;
    asm volatile ("movb %1, %b0" : "=r"(data) : "o"(*ptr) : "memory");
    return data;
}

inline void mmio_writel(uint32_t *ptr, uint32_t data)
{
    asm volatile("movl %k0, %1" :: "ri"(data), "o"(*ptr) : "memory");
}

inline void mmio_writeh(uint16_t *ptr, uint16_t data)
{
    asm volatile("movw %w0, %1" :: "ri"(data), "o"(*ptr) : "memory");
}

inline void mmio_writeb(uint8_t *ptr, uint8_t data)
{
    asm volatile("movb %b0, %1" :: "ri"(data), "o"(*ptr) : "memory");
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

#if defined(__x86_64__)
inline uint64_t mmio_readd(uint64_t *ptr)
{
    uint64_t data;
    asm volatile ("movq %1, %q0" : "=r"(data) : "o"(*ptr) : "memory");
    return data;
}
inline void mmio_writed(uint64_t *ptr, uint64_t data)
{
    asm volatile("movq %q0, %1" :: "r"(data), "o"(*ptr) : "memory");
}
#elif defined(__i386__)
inline uint64_t mmio_readd(uint64_t *ptr)
{
    uint32_t low, high;
    uint32_t * __attribute__((__may_alias__)) ptr32 = (uint32_t *)ptr;
    asm volatile ("movl %2, %k0 \n\t"
                  "movl %3, %k1"
                  : "=&r"(low), "=r"(high)
                  : "o"(*ptr32), "o"(*(ptr32 + 1)) : "memory");
    return ((uint64_t)high << 32) | low;
}
inline void mmio_writed(uint64_t *ptr, uint64_t data)
{
    uint32_t low = data & 0xFFFFFFFF;
    uint32_t high = data >> 32;
    uint32_t * __attribute__((__may_alias__)) ptr32 = (uint32_t *)ptr;
    asm volatile("movl %k0, %2 \n\t"
                 "movl %k1, %3"
                 :: "ri"(low), "ri"(high), "o"(*ptr32), "o"(*(ptr32 + 1)) : "memory");
}
#endif

#if defined(__riscv)
inline uint32_t mmio_readl(uint32_t *ptr)
{
    uint32_t data;
    asm volatile ("lw %0, %1" : "=r"(data) : "o"(*ptr));
    return data;
}
inline uint16_t mmio_readh(uint16_t *ptr)
{
    uint16_t data;
    asm volatile ("lh %0, %1" : "=r"(data) : "o"(*ptr));
    return data;
}
inline uint8_t mmio_readb(uint8_t *ptr)
{
    uint8_t data;
    asm volatile ("lb %0, %1" : "=r"(data) : "o"(*ptr));
    return data;
}
inline uint64_t mmio_readd(uint64_t *ptr)
{
    uint64_t data;
    asm volatile ("ld %0, %1" : "=r"(data) : "o"(*ptr));
    return data;
}

inline void mmio_writel(uint32_t *ptr, uint32_t data)
{
    asm volatile("sw %0, %1" :: "r"(data), "o"(*ptr));
}
inline void mmio_writeh(uint16_t *ptr, uint16_t data)
{
    asm volatile("sh %0, %1" :: "r"(data), "o"(*ptr));
}
inline void mmio_writeb(uint8_t *ptr, uint8_t data)
{
    asm volatile("sb %0, %1" :: "r"(data), "o"(*ptr));
}
inline void mmio_writed(uint64_t *ptr, uint64_t data)
{
    asm volatile("sd %0, %1" :: "r"(data), "o"(*ptr));
}

#define pci_mmio_readl mmio_readl
#define pci_mmio_writel mmio_writel
#endif

#if defined(__loongarch__)

inline void mmio_writeb(uint8_t *ptr, uint8_t data)
{
    asm volatile("st.b %0, %1" :: "r"(data), "o"(*ptr): "memory");
}

inline void mmio_writeh(uint16_t *ptr, uint16_t data)
{
    asm volatile("st.h %0, %1" :: "r"(data), "o"(*ptr): "memory");
}
inline void mmio_writel(uint32_t *ptr, uint32_t data)
{
    asm volatile("st.w %0, %1" :: "r"(data), "o"(*ptr): "memory");
}
inline void mmio_writed(uint64_t *ptr, uint64_t data)
{
    asm volatile("st.d %0, %1" :: "r"(data), "o"(*ptr) : "memory");
}


inline uint8_t mmio_readb(uint8_t *ptr)
{
    uint8_t data;
    asm volatile ("ld.b %0, %1" : "=r"(data) : "o"(*ptr) : "memory");
    return data;
}
inline uint16_t mmio_readh(uint16_t *ptr)
{
    uint16_t data;
    asm volatile ("ld.h %0, %1" : "=r"(data) : "o"(*ptr) : "memory");
    return data;
}
inline uint32_t mmio_readl(uint32_t *ptr)
{
    uint32_t data;
    asm volatile ("ld.w %0, %1" : "=r"(data) : "o"(*ptr): "memory");
    return data;
}
inline uint64_t mmio_readd(uint64_t *ptr)
{
    uint64_t data;
    asm volatile ("ld.d %0, %1" : "=r"(data) : "o"(*ptr): "memory");
    return data;
}

#define pci_mmio_readl mmio_readl
#define pci_mmio_writel mmio_writel
#endif

#endif