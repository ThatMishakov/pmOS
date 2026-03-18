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
#include <pmos/helpers.hh>
#include <pmos/pmbus_helper.hh>

extern pmos::Right device_right;
extern pmos::PortDispatcher dispatcher;
extern pmos::PMBUSHelper pmbus_helper;

extern std::string pci_string;

pmos::async::detached_task handle_ipc(AHCIPort &port);

pmos::async::task<uint64_t> publish_disk(AHCIPort &port, uint64_t sector_count,
                                         size_t logical_sector_size, size_t physical_sector_size)
{
    auto handler =
        DiskHandler::create(port, sector_count, logical_sector_size, physical_sector_size);
    pmos::utility::scope_guard guard {[=] { handler->destroy(); }};

    pmos::ipc::BUSObject object;
    object.set_name("system.disk." + pci_string + "_port" + std::to_string(port.index));
    object.set_property("sector_count", sector_count);
    object.set_property("logical_sector_size", logical_sector_size);
    object.set_property("physical_sector_size", physical_sector_size);
    object.set_property("handler_id", handler->get_disk_id());

    handle_ipc(port);

    auto right = port.port_right.clone();
    co_return co_await pmbus_helper.publish_object(std::move(object), std::move(right));
}