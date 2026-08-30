#include "tasks.hh"
#include <pmos/containers/hash_map.hh>
#include <cstddef>
#include <cstdint>

namespace kernel::proc
{

constexpr size_t FUTEX_HASH_BUCKETS = 256;

struct FutexHash {
    static std::size_t operator()(const TaskDescriptor &task) noexcept
    {
        std::size_t h = reinterpret_cast<std::uintptr_t>(task.page_table.get());

        h ^= static_cast<std::size_t>(task.futex_addr)
            + std::size_t{0x9e3779b9}
            + (h << 6)
            + (h >> 2);

        return h;
    }
};

pmos::containers::HashMap<TaskDescriptor, &TaskDescriptor::futex_list_node, FUTEX_HASH_BUCKETS, FutexHash> futex_hash_map;
Spinlock futex_hash_lock;

void TaskDescriptor::futex_push()
{
    Auto_Lock_Scope scope_lock(futex_hash_lock);

    assert(!futex_pushed);
    futex_pushed = true;
    futex_hash_map.insert(this);
}

void TaskDescriptor::futex_try_remove()
{
    Auto_Lock_Scope scope_lock(futex_hash_lock);

    if (futex_pushed) {
        futex_pushed = false;
        futex_hash_map.remove(this);
    }
}

} // namespace kernel::proc

