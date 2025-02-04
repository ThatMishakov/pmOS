#include "blockd.hh"

#include "disk_handler.hh"

#include <exception>
#include <inttypes.h>
#include <pmos/async/coroutines.hh>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/utility/scope_guard.hh>
#include <string_view>
#include <system_error>

static pmos_port_t blockd_port = 0;

bool WaitForBlockdPort::await_ready() noexcept { return blockd_port != 0; }
pmos_port_t WaitForBlockdPort::await_resume() noexcept { return blockd_port; }

using list_type =
    pmos::containers::CircularDoubleList<WaitForBlockdPort, &WaitForBlockdPort::l>;
static list_type waiters {};
static bool blockd_port_requested = false;
extern pmos_port_t ahci_port;

void WaitForBlockdPort::await_suspend(std::coroutine_handle<> hh)
{
    this->h = hh;

    if (!blockd_port_requested) {
        auto result =
            request_named_port(blockd_port_name.data(), blockd_port_name.size(), ahci_port, 0);
        if (result != SUCCESS)
            throw std::system_error(-result, std::system_category());
        blockd_port_requested = true;
    }

    waiters.push_back(this);
}

void blockd_port_ready(pmos_port_t port)
{
    blockd_port            = port;
    list_type::iterator it = waiters.begin();
    while (it != waiters.end()) {
        waiters.remove(it);
        it->h.resume();
        it = waiters.begin();
    }
}

bool RegisterDisk::await_ready() noexcept { return false; }

void RegisterDisk::await_suspend(std::coroutine_handle<> hh) { handler.h = hh; }

void RegisterDisk::await_resume() {}

pmos::async::task<uint64_t> register_disk(int port, uint64_t sector_count,
                                          size_t logical_sector_size, size_t physical_sector_size)
{
    auto blockd_port = co_await WaitForBlockdPort {};

    auto handler =
        DiskHandler::create(port, sector_count, logical_sector_size, physical_sector_size);
    pmos::utility::scope_guard guard {[=] { handler->destroy(); }};

    IPC_Disk_Register request = {
        .type                 = IPC_Disk_Register_NUM,
        .flags                = 0,
        .reply_port           = ahci_port,
        .disk_port            = ahci_port,
        .disk_id              = handler->get_disk_id(),
        .task_group_id        = pmos_process_task_group(),
        .sector_count         = sector_count,
        .logical_sector_size  = static_cast<uint32_t>(logical_sector_size),
        .physical_sector_size = static_cast<uint32_t>(physical_sector_size),
    };

    auto result = send_message_port(blockd_port, sizeof(request), (unsigned char *)&request);
    if (result != SUCCESS)
        throw std::system_error(-result, std::system_category());

    co_await RegisterDisk {*handler};

    if (handler->error_code != 0)
        throw std::system_error(-handler->error_code, std::system_category());

    guard.dismiss();
    co_return handler->get_disk_id();
}

void handle_register_disk_reply(const IPC_Disk_Register_Reply *reply)
{
    try {
        auto id      = reply->disk_id;
        auto handler = DiskHandler::get(id);
        handler->diskd_event_port     = reply->disk_port;
        handler->blockd_task_group_id = reply->task_group_id;
        handler->error_code           = reply->result_code;

        handler->h.resume();
    } catch (std::exception &e) {
        printf("Exception in handle_register_disk_reply: %s\n", e.what());
    }
}