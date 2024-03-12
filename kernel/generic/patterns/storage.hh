#pragma once
#include <lib/splay_tree_map.hh>

template <typename T>
class global_weak_storage {
private:
    static splay_tree_map<u64, weak_ptr<T>> storage;
    static Spinlock storage_lock;

    static u64 next_id = 1;
    u64 id = __atomic_fetch_add(&next_id, 1, __ATOMIC_SEQ_CST);

    void remove() {
        Auto_Lock_Scope scope_lock(storage_lock);
        storage.erase(id);
    }

    ~global_weak_storage() {
        remove();
    }
protected:
    // Can't be called from the constructor
    static void insert(const shared_ptr<T>& ptr) {
        Auto_Lock_Scope scope_lock(storage_lock);
        storage.insert(id, ptr);
    }
};