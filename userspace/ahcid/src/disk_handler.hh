#pragma once
#include <coroutine>
#include <cstddef>
#include <pmos/async/coroutines.hh>
#include <pmos/containers/intrusive_bst.hh>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <stdint.h>

struct AHCIPort;

pmos::async::detached_task handle_ipc(AHCIPort &port, uint64_t sector_count, size_t logical_sector_size, size_t physical_sector_size);

pmos::async::detached_task handle_disk_read(const Message_Descriptor &d, IPC_Disk_Read request);