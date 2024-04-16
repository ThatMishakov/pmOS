#include <deque>
#include <string>
#include <list>
#include <cstring>
#include <memory>
#include <pmos/ipc.h>
#include <pmos/ports.h>

constexpr size_t buffer_size = 8192;
std::deque<std::string> log_buffer;

std::list<pmos_port_t> log_output_ports;

pmos_port_t main_port = []() -> auto
{
    ports_request_t request = create_port(PID_SELF, 0);
    if (request.result != SUCCESS)
        throw std::runtime_error("Failed to create port");
    return request.port;
}();

const std::string stdout_port_name = "/pmos/stdout";
const std::string stderr_port_name = "/pmos/stderr";

const std::string log_port_name = "/pmos/logd";

void send_log_port(const std::string &message, pmos_port_t port)
{
    std::unique_ptr<char[]> write = std::make_unique<char[]>(sizeof(IPC_Write_Plain) + message.size());
    IPC_Write_Plain *write_msg = reinterpret_cast<IPC_Write_Plain *>(write.get());

    write_msg->type = IPC_Write_Plain_NUM;
    memcpy(write_msg->data, message.c_str(), message.size());

    result_t r = send_message_port(port, sizeof(IPC_Write_Plain) + message.size(), write.get());
    if (r != SUCCESS) {
        throw std::runtime_error("Failed to send message to port");
    }
}

void log(std::string message) {
    log_buffer.push_back(std::move(message));
    if (log_buffer.size() > buffer_size)
        log_buffer.pop_front();

    auto &m = log_buffer.back();

    for (auto p = log_output_ports.begin(); p != log_output_ports.end(); ){
        try {
            send_log_port(m, *p);
            p++;
        } catch (const std::exception &e) {
            log_output_ports.erase(p++);
            log("Error: " + std::string(e.what()) + "\n");
        }
    }
}

void register_log_output(const Message_Descriptor &, const IPC_Register_Log_Output &reg)
{
    // TODO: This throws
    log_output_ports.push_back(reg.log_port);

    IPC_Log_Output_Reply reply = {
        .type = IPC_Log_Output_Reply_NUM,
        .flags = 0,
        .result_code = 0,
    };

    result_t r = send_message_port(reg.reply_port, sizeof(reply), &reply);
    if (r != SUCCESS) {
        log_output_ports.remove(reg.log_port);
        log("Error: Failed to send reply to log output registration\n");
        return;
    }

    try {
        for (const auto &message : log_buffer) {
            send_log_port(message, reg.log_port);
        }
    } catch (const std::exception &e) {
        log_output_ports.pop_back();
        log("Error: " + std::string(e.what()) + "\n");
    }
}

int main()
{
    set_log_port(main_port, 0);

    {
        result_t r = name_port(main_port, stdout_port_name.c_str(), stdout_port_name.length(), 0);
        if (r != SUCCESS) {
            std::string error = "terminald: Error " + std::to_string(r) + " naming port\n";
            log(std::move(error));
        }

        r = name_port(main_port, stderr_port_name.c_str(), stderr_port_name.length(), 0);
        if (r != SUCCESS) {
            std::string error = "terminald: Error " + std::to_string(r) + " naming port\n";
            log(std::move(error));
        }

        r = name_port(main_port, log_port_name.c_str(), log_port_name.length(), 0);
        if (r != SUCCESS) {
            std::string error = "terminald: Error " + std::to_string(r) + " naming port\n";
            log(std::move(error));
        }
    }

    while (1)
    {
        Message_Descriptor msg;
        syscall_get_message_info(&msg, main_port, 0);

        std::unique_ptr<char[]> msg_buff = std::make_unique<char[]>(msg.size+1);

        get_first_message(msg_buff.get(), 0, main_port);

        msg_buff[msg.size] = '\0';

        if (msg.size < sizeof(IPC_Generic_Msg)) {
            log("Warning: recieved very small message\n");
            break;
        }
        
        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(msg_buff.get());

        switch (ipc_msg->type) {
        case IPC_Write_Plain_NUM: {
            IPC_Write_Plain *str = reinterpret_cast<IPC_Write_Plain *>(ipc_msg);
            log(str->data);
            break;
        }
        case IPC_Register_Log_Output_NUM: {
            IPC_Register_Log_Output *reg = reinterpret_cast<IPC_Register_Log_Output *>(ipc_msg);
            if (msg.size < sizeof(IPC_Register_Log_Output) - 1) {
                log("Warning: IPC_Register_Log_Output_NUM too small\n");
                break;
            }
            register_log_output(msg, *reg);
            break;
        }
        default:
            log("Warning: Unknown message type " + std::to_string(ipc_msg->type) + " from task " + std::to_string(msg.sender) + "\n");
            break;
        }
    }
}