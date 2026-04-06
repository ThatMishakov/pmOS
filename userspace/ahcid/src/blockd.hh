#pragma once
#include <pmos/async/coroutines.hh>
#include <pmos/helpers.hh>
#include <pmos/containers/intrusive_list.hh>
#include <string_view>
#include <pmos/ipc.h>

struct AHCIPort;
pmos::async::task<uint64_t> publish_disk(AHCIPort &port, uint64_t sector_count, size_t logical_sector_size, size_t physical_sector_size);