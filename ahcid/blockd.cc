#include "blockd.hh"

#include "disk_handler.hh"
#include "port.hh"

#include <exception>
#include <inttypes.h>
#include <pmos/async/coroutines.hh>
#include <pmos/ipc.h>
#include <pmos/ipc/bus_object.hh>
#include <pmos/ports.h>
#include <pmos/utility/scope_guard.hh>
#include <string_view>
#include <system_error>

static pmos_port_t pmbus_port = 0;

bool WaitForPMBusPort::await_ready() noexcept { return pmbus_port != 0; }
pmos_port_t WaitForPMBusPort::await_resume() noexcept { return pmbus_port; }

using list_type = pmos::containers::CircularDoubleList<WaitForPMBusPort, &WaitForPMBusPort::l>;
static list_type waiters {};
static bool pmbus_port_requested = false;
extern pmos_port_t ahci_port;

void WaitForPMBusPort::await_suspend(std::coroutine_handle<> hh)
{
    this->h = hh;

    if (!pmbus_port_requested) {
        auto result =
            request_named_port(pmbus_port_name.data(), pmbus_port_name.size(), ahci_port, 0);
        if (result != SUCCESS)
            throw std::system_error(-result, std::system_category());
            
        pmbus_port_requested = true;
    }

    waiters.push_back(this);
}

void pmbus_port_ready(pmos_port_t port)
{
    pmbus_port            = port;
    list_type::iterator it = waiters.begin();
    while (it != waiters.end()) {
        waiters.remove(it);
        it->h.resume();
        it = waiters.begin();
    }
}

bool PublishDisk::await_ready() noexcept { return false; }

void PublishDisk::await_suspend(std::coroutine_handle<> hh) { handler.h = hh; }

void PublishDisk::await_resume() {}

pmos::async::task<uint64_t> publish_disk(AHCIPort &port, uint64_t sector_count, size_t logical_sector_size,
                  size_t physical_sector_size)
{
    auto pmbus_port = co_await WaitForPMBusPort {};

    auto handler =
        DiskHandler::create(port, sector_count, logical_sector_size, physical_sector_size);
    pmos::utility::scope_guard guard {[=] { handler->destroy(); }};

    pmos::ipc::BUSObject object;
    object.set_name("system.disk.todo_n" + std::to_string(port.index));
    object.set_property("sector_count", sector_count);
    object.set_property("logical_sector_size", logical_sector_size);
    object.set_property("physical_sector_size", physical_sector_size);
    object.set_property("handler_id", handler->get_disk_id());

    auto vec    = object.serialize_into_ipc(ahci_port, handler->get_disk_id(), ahci_port, pmos_process_task_group());
    auto result = send_message_port(pmbus_port, vec.size(), (unsigned char *)&vec[0]);
    if (result != SUCCESS)
        throw std::system_error(-result, std::system_category());

    co_await PublishDisk {*handler};

    if (handler->error_code != 0)
        throw std::system_error(-handler->error_code, std::system_category());
    guard.dismiss();
    co_return handler->get_disk_id();
}

// pmos::async::task<uint64_t> register_disk(AHCIPort &port, uint64_t sector_count,
//                                           size_t logical_sector_size, size_t physical_sector_size)
// {
//     auto blockd_port = co_await WaitForBlockdPort {};

//     auto handler =
//         DiskHandler::create(port, sector_count, logical_sector_size, physical_sector_size);
//     pmos::utility::scope_guard guard {[=] { handler->destroy(); }};

//     IPC_Disk_Register request = {
//         .type                 = IPC_Disk_Register_NUM,
//         .flags                = 0,
//         .reply_port           = ahci_port,
//         .disk_port            = ahci_port,
//         .disk_id              = handler->get_disk_id(),
//         .task_group_id        = pmos_process_task_group(),
//         .sector_count         = sector_count,
//         .logical_sector_size  = static_cast<uint32_t>(logical_sector_size),
//         .physical_sector_size = static_cast<uint32_t>(physical_sector_size),
//     };

//     auto result = send_message_port(blockd_port, sizeof(request), (unsigned char *)&request);
//     if (result != SUCCESS)
//         throw std::system_error(-result, std::system_category());

//     co_await RegisterDisk {*handler};

//     if (handler->error_code != 0)
//         throw std::system_error(-handler->error_code, std::system_category());

//     guard.dismiss();
//     co_return handler->get_disk_id();
// }

void handle_register_disk_reply(const IPC_Disk_Register_Reply *reply)
{
    try {
        auto id                       = reply->disk_id;
        auto handler                  = DiskHandler::get(id);
        handler->diskd_event_port     = reply->disk_port;
        handler->blockd_task_group_id = reply->task_group_id;
        handler->error_code           = reply->result_code;

        handler->h.resume();
    } catch (std::exception &e) {
        printf("Exception in handle_register_disk_reply: %s\n", e.what());
    }
}