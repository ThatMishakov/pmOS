#include "tasks.hh"
#include <pmos/containers/hash_map.hh>
#include <cstddef>
#include <cstdint>

namespace kernel::proc
{

constexpr size_t FUTEX_HASH_BUCKETS = 256;

struct FutexKey {
    paging::Arch_Page_Table *page_table;
    ulong futex_addr;

    bool operator==(const TaskDescriptor &task) const
    {
        return page_table == task.page_table.get() and futex_addr == task.futex_addr;
    }
};

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

    static std::size_t operator()(const FutexKey &key) noexcept
    {
        std::size_t h = reinterpret_cast<std::uintptr_t>(key.page_table);

        h ^= static_cast<std::size_t>(key.futex_addr)
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

void TaskDescriptor::futex_wake(ulong futex_addr, bool wake_all)
{
    auto list = pmos::containers::CircularDoubleList<TaskDescriptor, &TaskDescriptor::futex_list_node>{};

    {
        Auto_Lock_Scope scope_lock(futex_hash_lock);
        do {
            auto task = futex_hash_map.find(FutexKey {page_table.get(), futex_addr});

            if (!task)
                break;

            __atomic_or_fetch(&task->wake_reason_mask, TaskDescriptor::SCHED_WAKE_FUTEX, __ATOMIC_RELEASE);

            futex_hash_map.remove(task);
            list.push_back(task);
        } while (wake_all);
    }
    
    auto get_next_task = [&list]() -> TaskDescriptor * {
        Auto_Lock_Scope scope_lock(futex_hash_lock);

        if (list.empty())
            return nullptr;

        auto task = &list.front();
        list.erase(task);
        task->futex_pushed = false;

        return task;
    };
    
    while (auto task = get_next_task())
        task->atomic_do_deferred_unblock();
}

} // namespace kernel::proc

