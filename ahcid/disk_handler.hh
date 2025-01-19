#pragma once
#include <cstddef>
#include <stdint.h>
#include <coroutine>
#include <pmos/ports.h>

class DiskHandler
{
public:
    uint64_t get_disk_id() const { return disk_id; }

    static DiskHandler *create(int port, uint64_t sector_count, std::size_t logical_sector_size,
                               std::size_t physical_sector_size);
    void destroy() noexcept;
    static DiskHandler *get(uint64_t id);

    // TODO: maybe have a friend function instead of this mess...
    int error_code;
    std::coroutine_handle<> h;

    pmos_port_t diskd_event_port;
    uint64_t blockd_task_group_id;
protected:
    DiskHandler() = default;
    uint64_t disk_id;
    uint64_t sector_count;
    std::size_t logical_sector_size;
    std::size_t physical_sector_size;

    int port;
};