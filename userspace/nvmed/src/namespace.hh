#pragma once
#include <pmos/async/coroutines.hh>

struct NVMeNamespace {
    uint64_t size;
    unsigned block_size;
    unsigned id;
};

pmos::async::detached_task handle_namespace(uint32_t namespace_id);