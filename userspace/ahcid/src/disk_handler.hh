#pragma once
#include <coroutine>
#include <cstddef>
#include <pmos/async/coroutines.hh>
#include <pmos/containers/intrusive_bst.hh>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <stdint.h>

struct TaskGroup;
class DiskHandler;

struct TaskGroupOpenDisk {
    TaskGroup *group;
    DiskHandler *disk;

    size_t open_count;

    pmos::containers::RBTreeNode<TaskGroupOpenDisk> task_group_node;
    pmos::containers::RBTreeNode<TaskGroupOpenDisk> disk_node;
};

struct CompareByTaskGroup {
    static bool operator()(const TaskGroupOpenDisk &a, const TaskGroupOpenDisk &b);
    static bool operator()(uint64_t a, const TaskGroupOpenDisk &b);
    static bool operator()(const TaskGroupOpenDisk &a, uint64_t b);
};

struct CompareByDisk {
    static bool operator()(const TaskGroupOpenDisk &a, const TaskGroupOpenDisk &b);
    static bool operator()(uint64_t a, const TaskGroupOpenDisk &b);
    static bool operator()(const TaskGroupOpenDisk &a, uint64_t b);
};

struct TaskGroup {
    uint64_t task_group_id;
    pmos::containers::RedBlackTree<TaskGroupOpenDisk, &TaskGroupOpenDisk::disk_node,
                                   CompareByDisk>::RBTreeHead disk_tree;

    TaskGroup(uint64_t id): task_group_id(id) {}

    void handle_maybe_empty();
    void remove();
    static TaskGroup *get_or_create(uint64_t task_group_id);
};

struct AHCIPort;
class DiskHandler
{
public:
    uint64_t get_disk_id() const { return disk_id; }

    static DiskHandler *create(AHCIPort &port, uint64_t sector_count, std::size_t logical_sector_size,
                               std::size_t physical_sector_size);
    void destroy() noexcept;
    static DiskHandler *get(uint64_t id);

    // TODO: maybe have a friend function instead of this mess...
    int error_code;
    std::coroutine_handle<> h;

    pmos_port_t diskd_event_port;
    uint64_t blockd_task_group_id;

    pmos::containers::RedBlackTree<TaskGroupOpenDisk, &TaskGroupOpenDisk::task_group_node,
                                   CompareByTaskGroup>::RBTreeHead task_group_tree;

    uint64_t get_sector_count() const { return sector_count; }
    size_t get_logical_sector_size() const { return logical_sector_size; }

    AHCIPort &get_port();

protected:
    DiskHandler(AHCIPort &port, uint64_t disk_id, uint64_t sector_count, std::size_t logical_sector_size,
                std::size_t physical_sector_size);
    AHCIPort &port;
    uint64_t disk_id;
    uint64_t sector_count;
    std::size_t logical_sector_size;
    std::size_t physical_sector_size;
};

void handle_register_disk_reply(const Message_Descriptor &d, const IPC_Disk_Register_Reply *reply);
void handle_disk_open(const Message_Descriptor &d, const IPC_Disk_Open *request);

bool sender_is_of_group(uint64_t task_group_id, Message_Descriptor msg);

pmos::async::detached_task handle_disk_read(const Message_Descriptor &d, IPC_Disk_Read request);