#include <stdio.h>
#include <cstdint>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/helpers.h>
#include <pmos/memory.h>
#include <memory>
#include <string>
#include "ns16550.hh"
#include <queue>
#include <cstring>

// Either physcial memory base or I/O port base
uint64_t terminal_base = 0x0;
// 0x0 = MMIO, 0x1 = PIO
uint8_t access_type = 0x0;
uint8_t interface_type = 0x0;

unsigned baud_rate = 0;
int parity = 0;
int stop_bits = 0;
int flow_control = 0;
int terminal_type = 0;

int interrupt_type_mask = 0;
int gsi_num = 0;
int pc_irq = 0;

// TODO: Don't assume it, apparently it's not always that
unsigned clock_frequency = 1843200;

volatile uint8_t *serial_mapped = nullptr;

pmos_port_t serial_port = []() -> auto
{
    ports_request_t request = create_port(PID_SELF, 0);
    return request.port;
}();

constexpr std::string devicesd_port_name = "/pmos/devicesd";
pmos_port_t devicesd_port = []() -> auto
{
    ports_request_t request = get_port_by_name(devicesd_port_name.c_str(), devicesd_port_name.length(), 0);
    return request.port;
}();

uint8_t read_register(int index)
{
    // TODO: IO access for x86
    return *(serial_mapped + index);
}

void write_register(int index, uint8_t value)
{
    *(serial_mapped + index) = value;
}

void set_register(int index, uint8_t mask)
{
    uint8_t val = read_register(index);
    val |= mask;
    write_register(index, val);
}

void clear_register(int index, uint8_t mask)
{
    uint8_t val = read_register(index);
    val &= ~mask;
    write_register(index, val);
}

void ns16550_init()
{
    IPC_Request_Serial request = {
        .type = IPC_Request_Serial_NUM,
        .flags = 0,
        .reply_port = serial_port,
        .serial_id = 0,
    };

    result_t result = send_message_port(devicesd_port, sizeof(request), static_cast<void*>(&request));
    if (result != SUCCESS) {
        printf("Failed to send message to devicesd port %lx: %lx\n", devicesd_port, result);
        return;
    }

    Message_Descriptor desc;
    uint8_t* message;
    result = get_message(&desc, &message, serial_port);
    if (result != SUCCESS) {
        printf("Failed to get message from devicesd\n");
        return;
    }
    std::unique_ptr<unsigned char[]> message_ptr(message);

    IPC_Serial_Reply* reply = reinterpret_cast<IPC_Serial_Reply*>(message_ptr.get());
    if (reply->type != IPC_Serial_Reply_NUM) {
        printf("Invalid reply type: %i\n", reply->type);
        return;
    }

    if (reply->result < 0) {
        printf("Failed to initialize ns16550: recieved error %i\n", -reply->result);
        return;
    }

    terminal_base = reply->base_address;
    access_type = reply->access_type;
    interface_type = reply->interface_type;
    baud_rate = reply->baud_rate;
    parity = reply->parity;
    stop_bits = reply->stop_bits;
    flow_control = reply->flow_control;
    terminal_type = reply->terminal_type;
    interrupt_type_mask = reply->interrupt_type;
    gsi_num = reply->gsi_number;
    pc_irq = reply->pc_int_number;

    printf("Initialized ns16550 with base address 0x%lx, access type %i, interface type %i, baud rate %u, parity %i, stop bits %i, flow control %i, terminal type %i, interrupt type mask %i, gsi num %i, pc irq %i\n",
        terminal_base, access_type, interface_type, baud_rate, parity, stop_bits, flow_control, terminal_type, interrupt_type_mask, gsi_num, pc_irq);

    if (access_type == 0x0) { // System memory
        auto result = create_phys_map_region(PID_SELF, nullptr, 0x1000, PROT_READ|PROT_WRITE, reinterpret_cast<void*>(terminal_base));
        if (result.result != SUCCESS)
            throw std::runtime_error("Failed to map serial port");

        serial_mapped = reinterpret_cast<volatile uint8_t*>(result.virt_addr);
    } else if (access_type != 0x1) { // Not system memory and serial port
        throw std::runtime_error("Invalid access type");
    }

    // Initialize the serial port

    // Disable interrupts
    write_register(IER, 0x00);

    if (baud_rate != 0) { // Baud rate not set up by firmware
        // Set baud rate
        // set DLAB = 1
        set_register(LCR, DLAB_MASK);

        uint8_t prescaler_division = 0;
        uint16_t divisor_constant = 0;
        if (baud_rate > clock_frequency) {
            prescaler_division = baud_rate / clock_frequency;
            divisor_constant = 1;

            if (prescaler_division >= (1 << 4))
                throw std::runtime_error("Clock too high");
        } else {
            divisor_constant = clock_frequency/baud_rate;
        }

        write_register(DLL, divisor_constant);
        write_register(DLM, divisor_constant >> 8);
        write_register(PSD, prescaler_division);
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
    write_register(LCR, lcr);

    // Enable FIFO
    // TODO: Be compatible with 16450
    set_register(FCR, FIFO_ENABLE | RX_FIFO_RESET | TX_FIFO_RESET | TRIGGER_8CHAR);
}

int buff_length = 0;
constexpr int buff_capacity = 16;

struct buffer {
    std::unique_ptr<char[]> data;
    size_t pos;
    size_t length;
};

std::queue<buffer> write_queue;

buffer active_buffer;

void write_str(const char * str)
{
    while (*str != '\0') {
        while ((read_register(LSR) & LSR_TX_EMPTY) == 0);
        write_register(THR, *str);
        str++;
    }
}

void write_str(const std::string & str)
{
    write_str(str.c_str());
}

void write_blind(const char *str, size_t size)
{
    for (size_t i = 0; i < size; ++i)
        write_register(THR, str[i]);
}

void check_tx()
{
    if ((read_register(LSR) & LSR_TX_EMPTY) == 0)
        return;

    if (not active_buffer.data and not write_queue.empty()) {
        active_buffer = std::move(write_queue.front());
        write_queue.pop();
    }

    if (active_buffer.data) {
        size_t i = active_buffer.length - active_buffer.pos;
        if (i > 16)
            i = 16;

        write_blind(active_buffer.data.get() + active_buffer.pos, i);
        active_buffer.pos += i;
        if (active_buffer.pos == active_buffer.length)
            active_buffer = {};
    }
}

void check_rx()
{
    while ((read_register(LSR) & LSR_DATA_READY) != 0) {
        auto t = read_register(THR);
        putc(t, stdout);
        fflush(stdout);
    }
}

void check_buffers()
{
    check_tx();
    check_rx();
}

void poll()
{
    auto r = pmos_request_timer(serial_port, 100);
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

constexpr std::string log_port_name = "/pmos/logd";
pmos_port_t log_port = 0;

void request_logger_port()
{
    // TODO: Add a syscall for this
    pmos_syscall(SYSCALL_REQUEST_NAMED_PORT, log_port_name.c_str(), log_port_name.length(), serial_port, 0);
}

void react_named_port_notification(char *msg_buff, size_t size)
{
    IPC_Kernel_Named_Port_Notification *msg = (IPC_Kernel_Named_Port_Notification *)msg_buff;
    if (size < sizeof(IPC_Kernel_Named_Port_Notification))
        return;
    
    log_port = msg->port_num;

    IPC_Register_Log_Output reg = {
        .type = IPC_Register_Log_Output_NUM,
        .flags = 0,
        .reply_port = serial_port,
        .log_port = serial_port,
        .task_id = getpid(),
    };

    send_message_port(log_port, sizeof(reg), &reg);
}

int main()
{
    printf("Hello from ns16550!\n");

    ns16550_init();

    request_logger_port();

    poll();

    while (1)
    {
        Message_Descriptor msg;
        syscall_get_message_info(&msg, serial_port, 0);

        std::unique_ptr<char[]> msg_buff = std::make_unique<char[]>(msg.size);

        get_first_message(msg_buff.get(), 0, serial_port);

        if (msg.size < sizeof(IPC_Generic_Msg)) {
            write_str("Warning: recieved very small message\n");
            break;
        }
        
        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(msg_buff.get());

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
                msg.size,
            });
        }
            break;
        case IPC_Log_Output_Reply_NUM:
            // Ignore it :)
            // (TODO)
            break;
        case IPC_Kernel_Named_Port_Notification_NUM:
            react_named_port_notification(msg_buff.get(), msg.size);
            break;
        default:
            write_str("Warning: Unknown message type " + std::to_string(ipc_msg->type) + " from task " + std::to_string(msg.sender) + "\n");
            break;
        } 
    }
}