#include "messaging.hh"
#include <lib/string.hh>
#include <lib/splay_tree_map.hh>
#include <lib/memory.hh>
#include <lib/list.hh>
#include <lib/set.hh>

struct Named_Port_Desc {
    const klib::string name;
    klib::weak_ptr<Port> parent_port;
    klib::set<klib::weak_ptr<TaskDescriptor>> waiting_tasks;

    bool present = false;
};

using named_ports_map = klib::splay_tree_map<const klib::string&, klib::shared_ptr<Named_Port_Desc>>;

struct Named_Port_Storage {
    named_ports_map storage;
    Spinlock lock;
};

extern Named_Port_Storage global_named_ports;