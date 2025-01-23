#include <cstdio>
#include <map>
#include <pmos/async/coroutines.hh>
#include <pmos/containers/intrusive_bst.hh>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <pmos/utility/scope_guard.hh>
#include <stdio.h>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

void ipc_send(const auto &data, pmos_port_t dest)
{
    auto size   = sizeof(data);
    auto result = send_message_port(dest, size, reinterpret_cast<const char *>(&data));
    if (result != 0)
        throw std::system_error(-result, std::system_category(), strerror(-result));
}

bool sender_is_of_group(uint64_t task_group_id, Message_Descriptor msg)
{
    auto [code, result] = is_task_group_member(msg.sender, task_group_id);
    return code == 0 && result == 1;
}

pmos_port_t create_main_port(std::string_view port_name)
{
    ports_request_t request = create_port(TASK_ID_SELF, 0);
    if (request.result != SUCCESS)
        throw std::system_error(-request.result, std::system_category());

    auto port = request.port;

    result_t r = name_port(port, port_name.data(), port_name.size(), 0);
    if (r != SUCCESS)
        throw std::system_error(-r, std::system_category());

    return port;
}

struct Disk;
struct DiskOpenAwaiter {
    Disk *disk;
    pmos::containers::RBTreeNode<DiskOpenAwaiter> node;
    uint64_t task_group_id;

    std::coroutine_handle<> h;
    IPC_Disk_Open_Reply *reply;

    bool opened = false;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h_);
    IPC_Disk_Open_Reply *await_resume() noexcept { return reply; }
};

DiskOpenAwaiter open_disk(Disk &disk, uint64_t task_group_id);

pmos_port_t main_port = create_main_port("/pmos/blockd");

struct Disk {
    size_t id;
    bool used;

    uint64_t driver_process_group;
    uint64_t driver_id;

    pmos_port_t driver_port;

    int logical_sector_size;
    int physical_sector_size;
    uint64_t sector_count;

    pmos::containers::RedBlackTree<
        DiskOpenAwaiter, &DiskOpenAwaiter::node,
        detail::TreeCmp<DiskOpenAwaiter, uint64_t, &DiskOpenAwaiter::task_group_id>>::RBTreeHead
        open_awaiters;
};

std::vector<Disk> disks;

size_t prepare_unused_disk_index()
{
    for (size_t i = 0; i < disks.size(); ++i) {
        if (not disks[i].used)
            return i;
    }

    disks.push_back({});
    return disks.size() - 1;
}

std::map<std::pair<uint64_t, uint64_t>, size_t> index_by_port;

void register_disk_error(int code, uint64_t disk_id, pmos_port_t reply_port)
{
    IPC_Disk_Register_Reply reply = {
        .type          = IPC_Disk_Register_Reply_NUM,
        .flags         = 0,
        .result_code   = static_cast<int16_t>(-code),
        .disk_id       = disk_id,
        .disk_port     = 0,
        .task_group_id = 0,
    };

    try {
        ipc_send(reply, reply_port);
    } catch (std::system_error &e) {
        printf("[blockd] Warning: Registering disk resulted in %i. Failed to send reply with %s\n",
               code, e.what());
    }
}

DiskOpenAwaiter open_disk(Disk &disk, uint64_t task_group_id)
{
    DiskOpenAwaiter awaiter;
    awaiter.task_group_id  = task_group_id;
    IPC_Disk_Open open_msg = {
        .type             = IPC_Disk_Open_NUM,
        .flags            = 0,
        .reply_port       = main_port,
        .disk_id          = disk.driver_id,
        .task_group_id    = disk.driver_process_group,
        .disk_consumer_id = pmos_process_task_group(),
    };

    ipc_send(open_msg, disk.driver_port);

    awaiter.reply = nullptr;
    awaiter.disk  = &disk;
    return awaiter;
}

void DiskOpenAwaiter::await_suspend(std::coroutine_handle<> h_)
{
    h = h_;
    disk->open_awaiters.insert(this);
}

pmos::async::detached_task register_disk(char *i, Message_Descriptor d)
{
    auto msg        = reinterpret_cast<IPC_Disk_Register *>(i);
    auto group_id   = msg->task_group_id;
    auto disk_id    = msg->disk_id;
    auto reply_port = msg->reply_port;

    if (!sender_is_of_group(group_id, d)) {
        register_disk_error(EPERM, disk_id, reply_port);
        co_return;
    }

    if (index_by_port.count({group_id, disk_id})) {
        register_disk_error(EEXIST, disk_id, reply_port);
        co_return;
    }

    try {
        auto index = prepare_unused_disk_index();

        auto &disk = disks[index];

        pmos::utility::scope_guard guard {[&] { disk.used = false; }};

        index_by_port.insert({{group_id, disk_id}, index});

        disk.used                 = true;
        disk.id                   = index;
        disk.driver_process_group = group_id;
        disk.driver_id            = disk_id;
        disk.driver_port          = msg->disk_port;
        disk.logical_sector_size  = msg->logical_sector_size;
        disk.physical_sector_size = msg->physical_sector_size;
        disk.sector_count         = msg->sector_count;

        IPC_Disk_Register_Reply reply = {
            .type          = IPC_Disk_Register_Reply_NUM,
            .flags         = 0,
            .result_code   = 0,
            .disk_id       = disk_id,
            .disk_port     = main_port,
            .task_group_id = pmos_process_task_group(),
        };

        ipc_send(reply, msg->reply_port);

        guard.dismiss();

        auto open_reply = co_await open_disk(disk, group_id);

        printf("[blockd] Disk %lu opened: %i\n", disk_id, open_reply->result_code);

        co_return;
    } catch (std::system_error &e) {
        register_disk_error(e.code().value(), disk_id, reply_port);
        co_return;
    }
}

int main()
{
    while (true) {
        Message_Descriptor msg;
        syscall_get_message_info(&msg, main_port, 0);

        auto msg_buff = std::make_unique<char[]>(msg.size);

        get_first_message(msg_buff.get(), 0, main_port);

        if (msg.size < sizeof(IPC_Generic_Msg)) {
            fprintf(stderr, "Warning: recieved very small message\n");
            break;
        }

        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(msg_buff.get());
        switch (ipc_msg->type) {
        case IPC_Disk_Register_NUM:
            register_disk(msg_buff.get(), msg);
            break;
        default:
            printf("Received message of type %d\n", ipc_msg->type);
        }
    }
}