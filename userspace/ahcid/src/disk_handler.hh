#pragma once
#include <coroutine>
#include <cstddef>
#include <pmos/async/coroutines.hh>
#include <pmos/containers/intrusive_bst.hh>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <stdint.h>
#include <pmos/helpers.hh>
#include <memory>
#include <set>
#include "port.hh"

struct AHCIPort;

pmos::async::detached_task handle_ipc(
    AHCIPort &port,
    std::shared_ptr<RRWrapper> recieve_right,
    uint64_t sector_count,
    size_t logical_sector_size,
    size_t physical_sector_size,
    uint64_t from_sector,
    uint64_t to_sector_count
);

pmos::async::detached_task handle_disk_read(const Message_Descriptor &d, IPC_Disk_Read request);