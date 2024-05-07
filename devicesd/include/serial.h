#ifndef SERIAL_H
#define SERIAL_H
#include <kernel/messaging.h>
#include <pmos/ipc.h>
#include <stdint.h>

struct serial_port {
    // 0 - mmio, 1 - io
    uint8_t access_type;

    // Access size
    uint8_t access_width;

    // Interface type
    // 0 - full 16550
    // 1 - 16450
    uint8_t interface_type;

    // Address
    // if interface_type is mmio, then pointer to physical memory
    // Otherwise, io port (e.g. 0x3F8 for COM1 on x86)
    uint64_t base_address;

    // Interrupt types bitmask
    // 0: dual-8259 IRQ
    // 1: I/O APIC
    // 2: I/O SAPIC
    // 3: ARMH GIC
    // 4: RISC-V PLIC/APLIC
    // 5-7: Reserved
    uint8_t interrupt;

    // PC-AT interrupt
    uint8_t pc_intno;

    // Global System Interrupt
    uint32_t gsi;

    // Configured baud rate
    // 0 - preconfigured, other values are baud rates
    uint32_t baud_rate;

    // Parity
    // 0 - none
    uint8_t parity;

    // Stop bits
    // 1 - 1 stop bit
    uint8_t stop_bits;

    // Flow control bitmask
    // bit 0: DCD required to send
    // bit 1: RTS/CTS hardware flow control
    // bit 2: XON/XOFF software flow control
    uint8_t flow_control;

    // Terminal type
    // 0 - VT100
    // 1 - VT100+
    // 2 - VT-UTF8
    // 3 - ANSI
    uint8_t terminal_type;
};

void init_serial();
void request_serial(Message_Descriptor *d, IPC_Request_Serial *m);

#endif