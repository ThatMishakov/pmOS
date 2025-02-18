#pragma once
#include <pmos/async/coroutines.hh>
#include <pmos/containers/intrusive_list.hh>
#include <string_view>
#include <pmos/ipc.h>

struct WaitForPMBusPort {
    pmos::containers::DoubleListHead<WaitForPMBusPort> l{};
    std::coroutine_handle<> h;

    void await_suspend(std::coroutine_handle<> h);
    bool await_ready() noexcept;
    pmos_port_t await_resume() noexcept;
};

inline std::string_view pmbus_port_name = "/pmos/pmbus";
void pmbus_port_ready(pmos_port_t port);

class DiskHandler;
struct AHCIPort;

struct PublishDisk {
    DiskHandler &handler; 

    void await_suspend(std::coroutine_handle<> h);
    bool await_ready() noexcept;
    void await_resume();
};

pmos::async::task<uint64_t> publish_disk(AHCIPort &port, uint64_t sector_count, size_t logical_sector_size, size_t physical_sector_size);

void handle_register_disk_reply(const IPC_Disk_Register_Reply *reply);