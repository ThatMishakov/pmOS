#include "ns16550.hh"

#include <cstdint>
#include <cstring>
#include <inttypes.h>
#include <memory>
#include <pmos/helpers.h>
#include <pmos/helpers.hh>
#include <pmos/interrupts.h>
#include <pmos/ipc.h>
#include <pmos/memory.h>
#include <pmos/ports.h>
#include <pmos/special.h>
#include <pmos/system.h>
#include <queue>
#include <stdio.h>
#include <string>

// Either physcial memory base or I/O port base
uint64_t terminal_base = 0x0;
// 0x0 = MMIO, 0x1 = PIO
uint8_t access_type    = 0x0;
uint8_t interface_type = 0x0;

unsigned baud_rate = 0;
int parity         = 0;
int stop_bits      = 0;
int flow_control   = 0;
int terminal_type  = 0;

int interrupt_type_mask = 0;
int gsi_num             = 0;
int pc_irq              = 0;

// TODO: Don't assume it, apparently it's not always that
unsigned clock_frequency = 1843200;

bool have_interrupts = false;

auto new_port()
{
    ports_request_t request = create_port(TASK_ID_SELF, 0);
    return request.port;
};

auto serial_port = pmos::Port::create().value();
auto reply_port  = pmos::Port::create().value();

constexpr auto devicesd_port_name = "/pmos/devicesd";
auto devicesd_right               = pmos::get_right_by_name(devicesd_port_name).value();

class IORW
{
public:
    virtual void write_register(int index, uint8_t value) = 0;
    virtual uint8_t read_register(int index)              = 0;
    virtual ~IORW()                                       = default;
};

class MMIO final: public IORW
{
public:
    MMIO(uint64_t base)
    {
        auto offset       = base & (PAGE_SIZE - 1);
        auto base_aligned = base & ~(uint64_t)(PAGE_SIZE - 1);

        auto result = create_phys_map_region(TASK_ID_SELF, nullptr, 0x1000, PROT_READ | PROT_WRITE,
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

std::unique_ptr<IORW> io_rw = nullptr;

void poll();

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

u32 int_vec = 0;

void set_up_interrupt()
{
    printf("Initializing interrupts... Mask: %x\n", interrupt_type_mask);

    // Check if interrupts are supported
    if (interrupt_type_mask & 0x01) {
        IPC_Reg_Int r = {
            .type      = IPC_Reg_Int_NUM,
            .flags     = IPC_Reg_Int_FLAG_EXT_INTS,
            .intno     = static_cast<uint32_t>(pc_irq),
            .int_flags = 0, // TODO
            .dest_task = get_task_id(),
            .dest_chan = serial_port.get(),
        };

        auto send_result = pmos::send_message_right_one(devicesd_right, r,
                                                        {&reply_port, pmos::RightType::SendOnce});
        if (!send_result) {
            printf("Failed to send message to set up interrupt: %i (%s)\n", send_result.error(),
                   strerror(send_result.error()));
            return;
        }

        Message_Descriptor desc;
        uint8_t *message;
        result_t result = 0;
        for (int i = 0; i < 5; ++i) {
            result = get_message(&desc, &message, reply_port.get(), NULL, NULL);
            if ((int)result == -EINTR)
                continue;
            break;
        }
        if (result != SUCCESS) {
            printf("Failed to get message from devicesd: %li (%s)\n", result, strerror(-result));
            return;
        }

        std::unique_ptr<unsigned char> message_ptr(message);

        IPC_Reg_Int_Reply *reply = reinterpret_cast<IPC_Reg_Int_Reply *>(message_ptr.get());
        if (reply->type != IPC_Reg_Int_Reply_NUM) {
            printf("Invalid reply type: %i\n", reply->type);
            return;
        }

        if (reply->status < 0) {
            printf("Failed to set up interrupt: recieved error %i\n", -reply->status);
            return;
        }

        int_vec = reply->intno;

        printf("ns16550d: Interrupt set up successfully, intno: %i\n", int_vec);

        have_interrupts = true;
    } else {
        unsigned flags = 0;

// TODO: I haven't found where to get the interrupt trigger type in SPCR table...
#ifdef __loongarch__
        flags = INTERRUPT_FLAG_LEVEL_TRIGGERED;
#endif

        auto [result, cpu, vector] = allocate_interrupt(gsi_num, flags);
        if (result) {
            printf("ns16550: failed to get interrupt, error %li\n", result);
            return;
        }

        result = set_affinity(0, cpu, 0);
        if (result) {
            printf("ns16550: failed to set affinity, error %li\n", result);
            return;
        }

        auto result2 = set_interrupt(serial_port.get(), vector, 0);
        if (result2.result) {
            printf("ns16550: failed to set interrupt, error %li\n", result2.result);
            return;
        }

        int_vec = vector;

        have_interrupts = true;
    }
}

void ns16550_init()
{
    // Request a high priority, since we are a driver
    // TODO: There is a bug where setting a priority makes the task disappear after some time :)
    request_priority(4);

    IPC_Request_Serial request = {
        .type      = IPC_Request_Serial_NUM,
        .flags     = 0,
        .serial_id = 0,
    };

    auto result = send_message_right(devicesd_right.get(), reply_port.get(),
                                     static_cast<void *>(&request), sizeof(request), nullptr, 0)
                      .result;
    if (result != SUCCESS) {
        printf("Failed to send message to devicesd right %" PRIu64 ": %i\n", devicesd_right.get(),
               (int)result);
        throw std::runtime_error("Failed to send message to devicesd");
    }

    Message_Descriptor desc;
    uint8_t *message;
    result = get_message(&desc, &message, reply_port.get(), NULL, NULL);
    if (result != SUCCESS) {
        printf("Failed to get message from devicesd\n");
        throw std::runtime_error("Failed to get message from devicesd");
    }
    std::unique_ptr<unsigned char> message_ptr(message);

    IPC_Serial_Reply *reply = reinterpret_cast<IPC_Serial_Reply *>(message_ptr.get());
    if (reply->type != IPC_Serial_Reply_NUM) {
        printf("Invalid reply type: %i\n", reply->type);
        throw std::runtime_error("Invalid reply type");
    }

    if (reply->result < 0) {
        printf("Failed to initialize ns16550: recieved error %i\n", -reply->result);
        throw std::runtime_error("Failed to initialize ns16550");
    }

    terminal_base       = reply->base_address;
    access_type         = reply->access_type;
    interface_type      = reply->interface_type;
    baud_rate           = reply->baud_rate;
    parity              = reply->parity;
    stop_bits           = reply->stop_bits;
    flow_control        = reply->flow_control;
    terminal_type       = reply->terminal_type;
    interrupt_type_mask = reply->interrupt_type;
    gsi_num             = reply->gsi_number;
    pc_irq              = reply->pc_int_number;

    printf("Initialized ns16550 with base address 0x%lx, access type %i, interface type %i, baud "
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

    if (have_interrupts) {
        // Enable interrupts
        set_register(IER, IER_RLS | IER_RX_DATA | IER_TX_EMPTY);
    } else {
        poll();
    }

    // Enable FIFO
    // TODO: Be compatible with 16450
    set_register(FCR, FIFO_ENABLE | RX_FIFO_RESET | TX_FIFO_RESET | TRIGGER_8CHAR);
}

int buff_length             = 0;
constexpr int buff_capacity = 16;

bool writing = false;
struct buffer {
    std::vector<std::byte> data;
    size_t pos;
    size_t length;
};

std::queue<buffer> write_queue;

buffer active_buffer;

void write_str(const char *str)
{
    while (*str != '\0') {
        while ((io_rw->read_register(LSR) & LSR_TX_EMPTY) == 0)
            ;
        io_rw->write_register(THR, *str);
        str++;
    }
}

void write_str(const std::string &str) { write_str(str.c_str()); }

void write_blind(const char *str, size_t size)
{
    for (size_t i = 0; i < size; ++i)
        io_rw->write_register(THR, str[i]);
}

void check_tx()
{
    if ((io_rw->read_register(LSR) & LSR_TX_EMPTY) == 0)
        return;

    if (active_buffer.data.empty() and not write_queue.empty()) {
        active_buffer = std::move(write_queue.front());
        write_queue.pop();
    }

    if (not active_buffer.data.empty()) {
        size_t i = active_buffer.length - active_buffer.pos;
        if (i > 16)
            i = 16;

        write_blind(reinterpret_cast<const char *>(active_buffer.data.data()) + active_buffer.pos, i);
        active_buffer.pos += i;
        if (active_buffer.pos == active_buffer.length)
            active_buffer = {};
    }
}

void write_interrupt()
{
    if (active_buffer.data.empty() and not write_queue.empty()) {
        active_buffer = std::move(write_queue.front());
        write_queue.pop();
    }

    if (!active_buffer.data.empty()) {
        size_t i = active_buffer.length - active_buffer.pos;
        if (i > 16)
            i = 16;

        writing = true;

        write_blind(reinterpret_cast<const char *>(active_buffer.data.data()) + active_buffer.pos, i);
        active_buffer.pos += i;
        if (active_buffer.pos == active_buffer.length)
            active_buffer = {};
    } else {
        writing = false;
        io_rw->read_register(ISR);
    }
}

void check_rx()
{
    while ((io_rw->read_register(LSR) & LSR_DATA_READY) != 0) {
        auto t = io_rw->read_register(THR);

        // putc(t, stdout);
        printf("\033[1;36m"
               "ns16550: Received letter %c"
               "\033[0m"
               "\n",
               t);
    }
    fflush(stdout);
}

void check_buffers()
{
    check_tx();
    check_rx();
}

void poll()
{
    auto r = pmos_request_timer(serial_port.get(), 100, 0);
    if (r != 0) {
        printf("Failed to request timer\n");
        return;
    }
}

void react_timer_msg()
{
    poll();
    check_buffers();
}

std::string log_right_name = "/pmos/logd";
pmos::Right log_right;

void request_logger_port()
{
    // TODO: Add a syscall for this
    request_named_port(log_right_name.c_str(), log_right_name.length(), serial_port.get(), 0);
}

void react_named_port_notification(const char *msg_buff, size_t size, pmos::Right right)
{
    IPC_Kernel_Named_Port_Notification *msg = (IPC_Kernel_Named_Port_Notification *)msg_buff;
    if (size < sizeof(IPC_Kernel_Named_Port_Notification))
        return;

    log_right = std::move(right);

    IPC_Register_Log_Output reg = {
        .type       = IPC_Register_Log_Output_NUM,
        .flags      = 0,
        .task_id    = get_task_id(),
    };

    auto serial_right = serial_port.create_right(pmos::RightType::SendMany).value().first;

    pmos::send_message_right_one(log_right, reg, {&serial_port, pmos::RightType::SendOnce}, false, std::move(serial_right)).value();
}

void react_interrupt()
{
    u8 interrupt_status = io_rw->read_register(ISR);
    if ((interrupt_status & ISR_INT_PENDING) == 0) {
        switch (interrupt_status & ISR_MASK) {
        case 0b0110: // Receiver Line Status
            // Acknowledge by reading LSR
            io_rw->read_register(LSR);
            break;
        case 0b0100: // Received Data Available
            check_rx();
            break;
        case 0b1100: // Reception Timeout
            check_rx();
            break;
        case 0b0010: // Transmitter Holding Register Empty
            write_interrupt();
            break;
        }
    } // else: Spurious interrupt

    complete_interrupt(int_vec);
}

int main()
{
    printf("Hello from ns16550! My task id: %li\n", get_task_id());

    ns16550_init();

    write_str("!! ns16550 task id: " + std::to_string(get_task_id()) + " !!\n");

    request_logger_port();

    while (1) {
        auto [msg, msg_buff, reply_right, array] = serial_port.get_first_message().value();

        if (msg.size < sizeof(IPC_Generic_Msg)) {
            write_str("Warning: recieved very small message\n");
            break;
        }

        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(msg_buff.data());

        switch (ipc_msg->type) {
        case IPC_Timer_Reply_NUM: {
            react_timer_msg();
            break;
        }
        case IPC_Write_Plain_NUM: {
            if (msg.size <= sizeof(IPC_Write_Plain))
                break;

            write_queue.push({
                std::move(msg_buff),
                sizeof(IPC_Write_Plain),
                (size_t)msg.size,
            });
            if (!writing)
                check_tx();
        } break;
        case IPC_Log_Output_Reply_NUM:
            // Ignore it :)
            // (TODO)
            break;
        case IPC_Kernel_Named_Port_Notification_NUM:
            react_named_port_notification(reinterpret_cast<const char *>(msg_buff.data()), msg.size, std::move(array[0]));
            break;
        case IPC_Kernel_Interrupt_NUM:
            react_interrupt();
            break;
        default:
            write_str("Warning: Unknown message type " + std::to_string(ipc_msg->type) +
                      " from task " + std::to_string(msg.sender) + "\n");
            break;
        }
    }
}