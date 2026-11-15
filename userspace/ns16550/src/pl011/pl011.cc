#include "pl011.hh"
#include <cstdint>
#include "../common.hh"
#include <pmos/io.h>
#include <unistd.h>
#include <pmos/memory.h>
#include <pmos/system.h>
#include <sys/mman.h>
#include <sched.h>

constexpr unsigned UARTDR = 0x00/sizeof(uint32_t);
constexpr unsigned UARTFR = 0x18/sizeof(uint32_t);
constexpr unsigned UARTIBRD = 0x24/sizeof(uint32_t);
constexpr unsigned UARTFBRD = 0x28/sizeof(uint32_t);
constexpr unsigned UARTLCR_H = 0x2c/sizeof(uint32_t);
constexpr unsigned UARTCR = 0x30/sizeof(uint32_t);
constexpr unsigned UARTIFLS = 0x34/sizeof(uint32_t);
constexpr unsigned UARTIMSC = 0x38/sizeof(uint32_t);

constexpr unsigned UARTMIS = 0x40/sizeof(uint32_t);
constexpr unsigned UARTICR = 0x44/sizeof(uint32_t);

constexpr u16 UARTFR_BUSY = 1 << 3;
constexpr u16 UARTFR_RXFE = 1 << 4;
constexpr u16 UARTFR_TXFF = 1 << 5;

constexpr u16 UARTCR_TXE = 1 << 8;
constexpr u16 UARTCR_RXE = 1 << 9;
constexpr u16 UARTCR_UARTEN = 1 << 0;

constexpr u16 RXIM = 1 << 4;
constexpr u16 TXIM = 1 << 5;
constexpr u16 RTIM = 1 << 6;

namespace {

struct PL011 final: public UART
{
    virtual void write_string_blocking(const char *str) override;
    virtual void set_interrupts() override;
    virtual size_t write(std::span<std::byte> data) override;
    virtual std::vector<std::byte> read() override;
    virtual void react_interrupt() override;
} pl011_instance;

uint32_t *virt_pl011_base = nullptr;
void map_pl011()
{
    const auto page_size = getpagesize();
    const auto offset = terminal_base & (page_size - 1);
    const auto base_aligned = terminal_base & ~(uint64_t)(page_size - 1);

    auto result = create_phys_map_region(TASK_ID_SELF, nullptr, page_size, PROT_READ | PROT_WRITE, base_aligned);
        if (result.result != SUCCESS)
            throw std::runtime_error("Failed to map serial port");

    virt_pl011_base = reinterpret_cast<uint32_t *>(result.virt_addr) + offset / sizeof(uint32_t);
}

void PL011::write_string_blocking(const char *str)
{
    while (*str != '\0') {
        while (mmio_readl(virt_pl011_base + UARTFR) & UARTFR_TXFF)
            sched_yield();
        mmio_writel(virt_pl011_base + UARTDR, *str);
        str++;
    }
}

size_t PL011::write(std::span<std::byte> data)
{
    size_t i = 0;
    while (i < data.size()) {
        if (mmio_readl(virt_pl011_base + UARTFR) & UARTFR_TXFF)
            break;
        mmio_writel(virt_pl011_base + UARTDR, static_cast<uint8_t>(data[i]));
        i++;
    }
    return i;
}

std::vector<std::byte> PL011::read()
{
    std::vector<std::byte> out{};

    while (!(mmio_readl(virt_pl011_base + UARTFR) & UARTFR_RXFE)) {
        auto t = mmio_readl(virt_pl011_base + UARTDR);
        out.push_back(static_cast<std::byte>(t));
    }

    return out;
}

void PL011::set_interrupts()
{
    u32 val = mmio_readl(virt_pl011_base + UARTIMSC);

    if (reading)
        val |= RXIM | RTIM; // RXIM
    else
        val &= ~RXIM & ~RTIM;

    if (writing)
        val |= TXIM; // TXIM
    else
        val &= ~TXIM;

    mmio_writel(virt_pl011_base + UARTIMSC, val);
}

void PL011::react_interrupt()
{
    u32 mis = mmio_readl(virt_pl011_base + UARTMIS);

    if (mis & RXIM || mis & RTIM) { // RXIM
        check_rx();
    }

    if (mis & TXIM) { // TXIM
        write_interrupt();
    }

    mmio_writel(virt_pl011_base + UARTICR, mis);
}

} // namespace

void init_pl011()
{
    uart = &pl011_instance;

    if (access_type != 0x0)
        throw std::runtime_error("PL011 only supports MMIO access type");

    map_pl011();

    mmio_writel(virt_pl011_base + UARTCR, 0x0); // Disable UART

    while (mmio_readl(virt_pl011_base + UARTFR) & UARTFR_BUSY)
        sched_yield();

    auto uartlcr_h = mmio_readl(virt_pl011_base + UARTLCR_H);
    uartlcr_h &= ~(1 << 4); // Clear FEN bit
    mmio_writel(virt_pl011_base + UARTLCR_H, uartlcr_h);

    if (baud_rate == 0) {
        const uint64_t divisor64 = (4ULL * clock_frequency + baud_rate / 2) / baud_rate;
        const uint16_t ibrd = divisor64 / 64;
        const uint16_t fbrd = divisor64 % 64;

        mmio_writel(virt_pl011_base + UARTIBRD, ibrd);
        mmio_writel(virt_pl011_base + UARTFBRD, fbrd);
    }

    auto uartifls = mmio_readl(virt_pl011_base + UARTIFLS);
    uartifls &= ~0x3f; // Clear RXIFLSEL and TXIFLSEL bits
    uartifls |= 0b010 << 0; // Set RXIFLSEL to 2 (1/2 full)
    uartifls |= 0b010 << 3; // Set TXIFLSEL to 2 (1/2 full)
    mmio_writel(virt_pl011_base + UARTIFLS, uartifls);

    uartlcr_h &= 0xffff0000;

    // Word length: 8 bits, no parity, one stop bit
    uartlcr_h |= (3 << 5); // WLEN = 3 for 8 bits
    uartlcr_h |= (1 << 4); // FEN = 1 for FIFO enabled
    mmio_writel(virt_pl011_base + UARTLCR_H, uartlcr_h);


    set_up_interrupt();

    // Enable UART
    mmio_writel(virt_pl011_base + UARTCR, UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE);
}