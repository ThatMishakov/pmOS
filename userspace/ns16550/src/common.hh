#pragma once
#include <cstdint>
#include <vector>
#include <span>

extern bool writing;
extern bool reading;

struct UART {
    virtual void write_string_blocking(const char *str) = 0;
    virtual void set_interrupts() = 0;
    virtual size_t write(std::span<std::byte> data) = 0;
    virtual std::vector<std::byte> read() = 0;

    virtual void react_interrupt() = 0;
};

extern UART *uart;

// Either physcial memory base or I/O port base
extern uint64_t terminal_base;
// 0x0 = MMIO, 0x1 = PIO
extern uint8_t access_type;
extern uint8_t interface_type;

extern unsigned baud_rate;
extern int parity;
extern int stop_bits;
extern int flow_control;
extern int terminal_type;

extern int interrupt_type_mask;
extern uint32_t gsi_num;
extern uint32_t pc_irq;

// TODO: Don't assume it, apparently it's not always that
extern unsigned clock_frequency;

extern bool have_interrupts;

void poll();
void set_up_interrupt();
void write_interrupt();
void check_rx();
