#pragma once
#include <memory>
#include <pmos/ports.h>
#include <unordered_map>
#include <atomic>

struct Pipe {
    static inline std::atomic<uint64_t> last_pipe_id = 1;

    struct ConsumerData {
        size_t refcount = 0;
    };

    std::unordered_map<uint64_t, ConsumerData> pipe_consumers;
    uint64_t task_id = 0;
    uint64_t pipe_port = 0;
    uint64_t pipe_id = last_pipe_id++;

    ConsumerData &register_new_consumer(uint64_t consumer_id, size_t refcount);
    void notify_unregister(uint64_t consumer_id);

    ~Pipe();
};

extern thread_local Pipe pipe_data;

void open_pipe(std::unique_ptr<char []> ipc_msg, Message_Descriptor desc);