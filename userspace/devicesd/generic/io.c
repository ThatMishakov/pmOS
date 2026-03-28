#include <pmos/io.h>

#if defined(__x86_64__) || defined(__i386__)
uint8_t io_in8(uint16_t port) {
    uint8_t data;
    __asm__ volatile("inb %1, %0" : "=a"(data) : "d"(port));
    return data;
}

void io_out8(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %0, %1" : : "a"(data), "d"(port));
}

uint16_t io_in16(uint16_t port) {
    uint16_t data;
    __asm__ volatile("inw %1, %0" : "=a"(data) : "d"(port));
    return data;
}

void io_out16(uint16_t port, uint16_t data) {
    __asm__ volatile("outw %0, %1" : : "a"(data), "d"(port));
}

uint32_t io_in32(uint16_t port) {
    uint32_t data;
    __asm__ volatile("inl %1, %0" : "=a"(data) : "d"(port));
    return data;
}

void io_out32(uint16_t port, uint32_t data) {
    __asm__ volatile("outl %0, %1" : : "a"(data), "d"(port));
}
#endif