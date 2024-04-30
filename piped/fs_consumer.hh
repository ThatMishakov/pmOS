#pragma once
#include <memory>
#include <set>
#include <unordered_map>
#include <cstdint>

class Pipe;
struct FsConsumer {
    std::set<pmos_port_t> pipe_ports;
};

extern std::unordered_map<uint64_t, FsConsumer> fs_consumers;

void register_pipe_with_consumer(uint64_t pipe_port, uint64_t consumer_id);
void unregister_pipe_with_consumer(uint64_t pipe_port, uint64_t consumer_id);
void handle_consumer_destroyed(uint64_t consumer_id);