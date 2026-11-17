#include "ns16550.hh"
#include "ns16550_regs.hh"
#include "../common.hh"
#include <pmos/memory.h>
#include <pmos/ports.h>
#include <pmos/special.h>
#include <pmos/system.h>
#include <cinttypes>
#include <sys/mman.h>


namespace
{

struct NS16550_UART final: public UART
{
    virtual void write_string_blocking(const char *str) override;
    virtual void set_interrupts() override;
    virtual size_t write(std::span<std::byte> data) override;
    virtual std::vector<std::byte> read() override;
    virtual void react_interrupt() override;
} ns16550_instance;

class IORW
{
public:
    virtual void write_register(int index, uint8_t value) = 0;
    virtual uint8_t read_register(int index)              = 0;
    virtual ~IORW()                                       = default;
};

std::unique_ptr<IORW> io_rw = nullptr;

class MMIO final: public IORW
{
public:
    MMIO(uint64_t base)
    {
        auto offset       = base & (getpagesize() - 1);
        auto base_aligned = base & ~(uint64_t)(getpagesize() - 1);

        auto result = create_phys_map_region(TASK_ID_SELF, nullptr, getpagesize(), PROT_READ | PROT_WRITE,
                                             base_aligned);
        if (result.result != SUCCESS)
            throw std::runtime_error("Failed to map serial port");

        serial_mapped = reinterpret_cast<volatile uint8_t *>(result.virt_addr) + offset;
    }

    void write_register(int index, uint8_t value) override
    {
        *(reinterpret_cast<volatile uint8_t *>(serial_mapped) + index) = value;
    }

    uint8_t read_register(int index) override
    {
        return *(reinterpret_cast<volatile uint8_t *>(serial_mapped) + index);
    }

private:
    volatile uint8_t *serial_mapped = nullptr;
};

class PORTIO final: public IORW
{
public:
    PORTIO(uint64_t base): base(base)
    {
#if defined(__i386__) || defined(__x86_64__)
        auto result = pmos_request_io_permission();
        if (result < 0)
            throw std::runtime_error("Couldn't request io permission");
#endif
    }

#if defined(__i386__) || defined(__x86_64__)
    void write_register(int index, uint8_t value) override
    {
        int port = base + index;
        __asm__ volatile("outb %b0, %w1" : : "a"(value), "Nd"(port));
    }

    uint8_t read_register(int index) override
    {
        uint8_t value;
        int port = base + index;
        __asm__ volatile("inb %w1, %b0" : "=a"(value) : "d"(port));
        return value;
    }
#else
    void write_register(int index, uint8_t value) override
    {
        throw std::runtime_error("IO not supported on the platform");
    }

    uint8_t read_register(int index) override
    {
        throw std::runtime_error("IO not supported on the platform");
    }
#endif
private:
    int base;
};

void set_register(int index, uint8_t mask)
{
    uint8_t val = io_rw->read_register(index);
    val |= mask;
    io_rw->write_register(index, val);
}

void clear_register(int index, uint8_t mask)
{
    uint8_t val = io_rw->read_register(index);
    val &= ~mask;
    io_rw->write_register(index, val);
}

int buff_length             = 0;
constexpr int buff_capacity = 16;

void NS16550_UART::set_interrupts()
{
    u8 mask = IER_RLS;
    if (reading)
        mask |= IER_RX_DATA;
    if (writing)
        mask |= IER_TX_EMPTY;

    io_rw->write_register(IER, mask);
}

void NS16550_UART::write_string_blocking(const char *str)
{
    while (*str != '\0') {
        while ((io_rw->read_register(LSR) & LSR_TX_EMPTY) == 0)
            ;
        io_rw->write_register(THR, *str);
        str++;
    }
}

void write_blind(const char *str, size_t size)
{
    for (size_t i = 0; i < size; ++i)
        io_rw->write_register(THR, str[i]);
}

size_t NS16550_UART::write(std::span<std::byte> data)
{
    if ((io_rw->read_register(LSR) & LSR_TX_EMPTY) == 0)
        return 0;

    size_t i = data.size();
    if (i > 16)
        i = 16;

    write_blind(reinterpret_cast<const char *>(data.data()), i);
    return i;
}

std::vector<std::byte> NS16550_UART::read()
{
    std::vector<std::byte> data;
    while ((io_rw->read_register(LSR) & LSR_DATA_READY) != 0) {
        auto t = io_rw->read_register(THR);
        data.push_back(static_cast<std::byte>(t));
    }
    return data;
}

void NS16550_UART::react_interrupt()
{
    u8 interrupt_status = io_rw->read_register(ISR);
    if ((interrupt_status & ISR_INT_PENDING) == 0) {
        switch (interrupt_status & ISR_MASK) {
        case 0b0110: // Receiver Line Status
            // Acknowledge by reading LSR
            io_rw->read_register(LSR);
            break;
        case 0b0100: // Received Data Available
        case 0b1100: // Reception Timeout
            check_rx();
            break;
        case 0b0010: // Transmitter Holding Register Empty
            write_interrupt();
            break;
        }
    } // else: Spurious interrupt
}

} // namespace

void ns16550_init()
{
    uart = &ns16550_instance;

    printf("Initialized ns16550 with base address 0x%" PRIx64 ", access type %i, interface type %i, baud "
           "rate %u, parity %i, stop bits %i, flow control %i, terminal type %i, interrupt type "
           "mask %i, gsi num %i, pc irq %i\n",
           terminal_base, access_type, interface_type, baud_rate, parity, stop_bits, flow_control,
           terminal_type, interrupt_type_mask, gsi_num, pc_irq);

    if (access_type == 0x0) { // System memory
        io_rw = std::make_unique<MMIO>(terminal_base);
    } else if (access_type == 0x01) {
        io_rw = std::make_unique<PORTIO>(terminal_base);
    } else { // Not system memory and serial port
        throw std::runtime_error("Invalid access type");
    }

    // Initialize the serial port

    // Disable interrupts
    io_rw->write_register(IER, 0x00);

    if (baud_rate != 0) { // Baud rate not set up by firmware
        // Set baud rate
        // set DLAB = 1
        set_register(LCR, DLAB_MASK);

        u32 right_freq = clock_frequency / 16;

        uint8_t prescaler_division = 0;
        uint16_t divisor_constant  = 0;
        if (baud_rate > right_freq) {
            prescaler_division = baud_rate / right_freq;
            divisor_constant   = 1;

            if (prescaler_division >= (1 << 4))
                throw std::runtime_error("Clock too high");
        } else {
            divisor_constant = right_freq / baud_rate;
        }

        io_rw->write_register(DLL, divisor_constant);
        io_rw->write_register(DLM, divisor_constant >> 8);
        io_rw->write_register(PSD, prescaler_division);
    }

    // Set DLAB = 0
    clear_register(LCR, DLAB_MASK);

    uint8_t lcr = 0;

    // Word length = 8 bits
    lcr |= 0b11 << 0;

    // Stop bits
    switch (stop_bits) {
    case 1:
        break;
    default:
        throw std::runtime_error("Unknown stop bits setting");
    }

    switch (parity) {
    case 0:
        break;
    default:
        throw std::runtime_error("Unknown parity setting");
    }

    // Set LCR
    io_rw->write_register(LCR, lcr);

    // Set up interrupt
    set_up_interrupt();

    // Enable FIFO
    // TODO: Be compatible with 16450
    io_rw->write_register(FCR, FIFO_ENABLE | RX_FIFO_RESET | TX_FIFO_RESET | TRIGGER_8CHAR);
}
