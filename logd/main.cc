#include <cstring>
#include <deque>
#include <list>
#include <memory>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <string>
#include <pmos/helpers.hh>

constexpr size_t buffer_size = 8192;
std::deque<std::string> log_buffer;

std::list<pmos::Right> log_output_rights;

pmos::Port main_port = pmos::Port::create().value();

const std::string stdout_port_name = "/pmos/stdout";
const std::string stderr_port_name = "/pmos/stderr";

const std::string log_port_name = "/pmos/logd";

void send_log_right(const std::string &message, pmos::Right &right)
{
    std::unique_ptr<char[]> write =
        std::make_unique<char[]>(sizeof(IPC_Write_Plain) + message.size());
    IPC_Write_Plain *write_msg = reinterpret_cast<IPC_Write_Plain *>(write.get());

    write_msg->type = IPC_Write_Plain_NUM;
    memcpy(write_msg->data, message.c_str(), message.size());

    auto r = send_message_right(right, std::span<const char>{write.get(), sizeof(IPC_Write_Plain) + message.size()}, {});
    if (!r) {
        throw std::runtime_error("Failed to send message to port");
    }
}

void log(std::string message)
{
    log_buffer.push_back(std::move(message));
    if (log_buffer.size() > buffer_size)
        log_buffer.pop_front();

    auto &m = log_buffer.back();

    for (auto p = log_output_rights.begin(); p != log_output_rights.end();) {
        try {
            send_log_right(m, *p);
            p++;
        } catch (const std::exception &e) {
            log_output_rights.erase(p++);
            log("Error: " + std::string(e.what()) + "\n");
        }
    }
}

void register_log_output(const Message_Descriptor &, const IPC_Register_Log_Output &reg, pmos::Right reply_right, pmos::Right data_right)
{
    int result = 0;

    if (!data_right || data_right.type() != pmos::RightType::SendMany)
        result = -EINVAL;

    IPC_Log_Output_Reply reply = {
        .type        = IPC_Log_Output_Reply_NUM,
        .flags       = 0,
        .result_code = result,
    };

    if (reply_right) {
        auto r = pmos::send_message_right_one(reply_right, reply, {}, true);
        if (!r) {
            log("Error: Failed to send reply to log output registration\n");
            return;
        }
    }

    if (result)
        return;

    try {
        for (const auto &message: log_buffer) {
            send_log_right(message, data_right);
        }
    } catch (const std::exception &e) {
        log("Error: " + std::string(e.what()) + "\n");
    }

    // TODO: This throws
    log_output_rights.push_back(std::move(data_right));
}

pmos_right_t stdout_right = INVALID_RIGHT;
pmos_right_t stderr_right = INVALID_RIGHT;
pmos_right_t log_right    = INVALID_RIGHT;

int main()
{
    set_log_port(main_port.get(), 0);
    log("\033[1;32m"
        "Hello from logd! My PID: " +
        std::to_string(get_task_id()) +
        "\033[0m\n"
        "\n");

    {
        right_request_t c = create_right(main_port.get(), &stdout_right, 0);
        if (c.result != SUCCESS) {
            std::string error = "terminald: Error " + std::to_string(c.result) + "creating rignt\n";
            log(std::move(error));
        }

        result_t r = name_right(c.right, stdout_port_name.c_str(), stdout_port_name.length(), 0);
        if (r != SUCCESS) {
            std::string error = "terminald: Error " + std::to_string(r) + " naming port\n";
            log(std::move(error));
        }

        c = create_right(main_port.get(), &stderr_right, 0);
        if (c.result != SUCCESS) {
            std::string error = "terminald: Error " + std::to_string(c.result) + "creating rignt\n";
            log(std::move(error));
        }


        r = name_right(c.right, stderr_port_name.c_str(), stderr_port_name.length(), 0);
        if (r != SUCCESS) {
            std::string error = "terminald: Error " + std::to_string(r) + " naming port\n";
            log(std::move(error));
        }

        c = create_right(main_port.get(), &log_right, 0);
        if (c.result != SUCCESS) {
            std::string error = "terminald: Error " + std::to_string(c.result) + "creating rignt\n";
            log(std::move(error));
        }


        r = name_right(c.right, log_port_name.c_str(), log_port_name.length(), 0);
        if (r != SUCCESS) {
            std::string error = "terminald: Error " + std::to_string(r) + " naming port\n";
            log(std::move(error));
        }
    }

    while (1) {
        auto [msg, data, right, reply_right] = main_port.get_first_message().value();

        if (msg.size < sizeof(IPC_Generic_Msg)) {
            log("Warning: recieved very small message\n");
            break;
        }

        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(data.data());

        switch (ipc_msg->type) {
        case IPC_Write_Plain_NUM: {
            IPC_Write_Plain *str = reinterpret_cast<IPC_Write_Plain *>(ipc_msg);
            log({str->data, msg.size - sizeof(*str)});
            break;
        }
        case IPC_Register_Log_Output_NUM: {
            IPC_Register_Log_Output *reg = reinterpret_cast<IPC_Register_Log_Output *>(ipc_msg);
            if (msg.size < sizeof(IPC_Register_Log_Output) - 1) {
                log("Warning: IPC_Register_Log_Output_NUM too small\n");
                break;
            }
            register_log_output(msg, *reg, std::move(right), std::move(reply_right[0]));
            break;
        }
        default:
            log("Warning: Unknown message type " + std::to_string(ipc_msg->type) + " from task " +
                std::to_string(msg.sender) + "\n");
            break;
        }
    }
}