#pragma once
#include <coroutine>
#include <cstddef>
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

    pmos::containers::RedBlackTree<TaskGroupOpenDisk, &TaskGroupOpenDisk::task_group_node,
                                   CompareByTaskGroup>::RBTreeHead task_group_tree;

protected:
    DiskHandler() = default;
    uint64_t disk_id;
    uint64_t sector_count;
    std::size_t logical_sector_size;
    std::size_t physical_sector_size;

    int port;
};

void handle_register_disk_reply(const Message_Descriptor &d, const IPC_Disk_Register_Reply *reply);
void handle_disk_open(const Message_Descriptor &d, const IPC_Disk_Open *request);

bool sender_is_of_group(uint64_t task_group_id, Message_Descriptor msg);