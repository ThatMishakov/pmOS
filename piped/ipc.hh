#pragma once
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <system_error>
#include <cstdint>

extern pmos_port_t main_port;
pmos_port_t create_port();

void send_message(pmos_port_t port, const auto &data)
{
    auto r = send_message_port(port, sizeof(data), reinterpret_cast<const void *>(&data));
    if (r != SUCCESS)
        throw std::system_error(-r, std::system_category());
}

void recieve_message(pmos_port_t port);

constexpr unsigned IPCRegisterConsumerType = 0xff000000;
struct IPCPipeRegisterConsumer {
    uint32_t type = IPCRegisterConsumerType;
    uint64_t reply_port = 0;
    uint64_t pipe_port = 0;
    uint64_t consumer_id = 0;
};

constexpr unsigned IPCPipeRegisterConsReplyType = 0xff000001;
struct IPCPipeRegisterConsReply {
    uint32_t type = IPCPipeRegisterConsReplyType;
    int32_t result = 0;
};

constexpr unsigned IPCNotifyUnregisterConsumerType = 0xff000002;
struct IPCNotifyUnregisterConsumer {
    uint32_t type = IPCNotifyUnregisterConsumerType;
    pmos_port_t pipe_port = 0;
    uint64_t consumer_id = 0;
};

