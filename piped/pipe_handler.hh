#pragma once
#include <atomic>
#include <deque>
#include <list>
#include <memory>
#include <pmos/ports.h>
#include <unordered_map>

struct MessageBuffer {
    std::unique_ptr<char[]> message;
    size_t start_position = 0;
    size_t size           = 0;
};

struct Pipe {
    static inline std::atomic<uint64_t> last_pipe_id = 1;

    struct ConsumerData {
        size_t reader_refcount = 0;
        size_t writer_refcount = 0;
        size_t wanted_bytes    = 0;
        size_t pending_count   = 0;
    };
    size_t reader_refcount = 0;
    size_t writer_refcount = 0;

    std::unordered_map<uint64_t, std::unique_ptr<ConsumerData>> pipe_consumers;
    uint64_t task_id   = 0;
    uint64_t pipe_port = 0;
    uint64_t pipe_id   = last_pipe_id++;

    std::deque<MessageBuffer> buffered_messages;
    size_t buffered_bytes = 0;

    struct PendingReader {
        uint64_t consumer_id;
        size_t max_bytes;
        pmos_port_t reply_port;
    };
    // Prefer list over deque because readers from the middle might be removed
    std::list<PendingReader> pending_readers;

    void fill_data(unsigned char *data, size_t bytes_to_read) noexcept;
    void pop_read_data(size_t bytes_to_pop) noexcept;

    ConsumerData &register_new_consumer(uint64_t consumer_id, size_t reader_refcount,
                                        size_t writer_refcount);
    void notify_unregister(uint64_t consumer_id);

    ~Pipe();
};

extern thread_local Pipe pipe_data;

void open_pipe(std::unique_ptr<char[]> ipc_msg, Message_Descriptor desc);