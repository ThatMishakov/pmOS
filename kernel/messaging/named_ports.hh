#include "messaging.hh"
#include <lib/string.hh>
#include <lib/splay_tree_map.hh>
#include <lib/memory.hh>
#include <lib/list.hh>
#include <lib/set.hh>

class Named_Port_Action {
public:
    virtual ~Named_Port_Action() = default;

    kresult_t virtual do_action(const klib::shared_ptr<Port>& port, const klib::string& name) = 0;
};

struct Named_Port_Desc: public Generic_Port {
    Named_Port_Desc(const klib::string& name, const klib::shared_ptr<Port> parent_port): name(name), parent_port(parent_port) {}
    Named_Port_Desc(klib::string&& name, const klib::shared_ptr<Port> parent_port): name(klib::forward<klib::string>(name)), parent_port(parent_port) {}

    const klib::string name;
    klib::weak_ptr<Port> parent_port;
    klib::vector<klib::unique_ptr<Named_Port_Action>> actions;
};

using named_ports_map = klib::splay_tree_map<const klib::string&, klib::shared_ptr<Named_Port_Desc>>;

struct Named_Port_Storage {
    named_ports_map storage;
    Spinlock lock;
};

extern Named_Port_Storage global_named_ports;

class Notify_Task : public Named_Port_Action {
public:
    virtual ~Notify_Task() override
    {
        do_action(nullptr, klib::string());
    }

    virtual kresult_t do_action(const klib::shared_ptr<Port>& port, const klib::string& name) override;

    Notify_Task(const klib::shared_ptr<TaskDescriptor>& t, const klib::shared_ptr<Generic_Port>& parent_port): task(t), parent_port(parent_port) {};
private:
    klib::weak_ptr<TaskDescriptor> task;
    klib::weak_ptr<Generic_Port> parent_port;
    bool did_action = false;
};

class Send_Message : public Named_Port_Action {
public:
    virtual ~Send_Message() override
    {
        do_action(nullptr, klib::string());
    }

    virtual kresult_t do_action(const klib::shared_ptr<Port>& port, const klib::string& name) override;

    Send_Message(const klib::shared_ptr<Port>& t): port(t) {};
private:
    klib::weak_ptr<Port> port;
    bool did_action = false;
};