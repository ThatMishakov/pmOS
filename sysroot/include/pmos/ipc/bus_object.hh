#pragma once

#include <cstdint>
#include <optional>
#include <pmos/ipc.h>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>

namespace pmos::ipc
{

class BUSObject
{
public:
    using property = std::variant<std::string, uint64_t>;

    /// Sets the property to the desired value (replacing the previous value if it had already been set)
    void set_property(std::string_view name, std::variant<std::string_view, uint64_t> p);

    /// Sets the name of this busobject
    void set_name(std::string name);

    std::optional<property> get_property(std::string_view name);

    /// Serializes the BUSObject into IPC_BUS_Publish_Object message
    std::vector<uint8_t> serialize(pmos_port_t handle_port, uint64_t task_group);
    std::vector<uint8_t> serialize_into_ipc(pmos_port_t reply_port, uint64_t user_arg, pmos_port_t handle_port, uint64_t task_group);
private:
    std::string name;
    std::unordered_map<std::string, property> properties;

    template<class... Ts>
    struct overloads : Ts... { using Ts::operator()...; };

    std::vector<uint8_t> serialize_properties();
};

inline void BUSObject::set_name(std::string name)
{
    std::swap(name, this->name);
}

inline void BUSObject::set_property(std::string_view name, std::variant<std::string_view, uint64_t> p)
{
    const auto visitor = overloads
    {
        [](std::string_view s) -> property { return std::string(s); },
        [](uint64_t i) -> property { return i;} ,
    };
    properties[std::string(name)] = std::visit(visitor, p);
}

inline std::vector<uint8_t> BUSObject::serialize_properties()
{
    const auto struct_size = sizeof(IPC_Object_Property);

    std::vector<uint8_t> result;
    for (const auto &p: properties) {
        if (auto val = std::get_if<std::string>(&p.second); val) {
            auto name_length = p.first.size() + 1;
            auto value_length = val->size() + 1;

            auto total_size = struct_size + name_length + value_length;
            auto alignment = (8 - (total_size % 8)) % 8;

            std::vector<uint8_t> buffer;
            buffer.reserve(total_size + alignment);

            IPC_Object_Property prop = {
                .length = total_size + alignment,
                .type = PROPERTY_TYPE_STRING,
                .data_start = struct_size + name_length,
            };
            auto ptr = reinterpret_cast<const uint8_t *>(&prop);
            buffer.insert(buffer.end(), ptr, ptr + sizeof(prop));
            auto name_ptr = reinterpret_cast<const uint8_t *>(p.first.c_str());
            buffer.insert(buffer.end(), name_ptr, name_ptr + name_length);
            auto data_ptr = reinterpret_cast<const uint8_t *>(val->c_str());
            buffer.insert(buffer.end(), data_ptr, data_ptr + value_length);
            buffer.insert(buffer.end(), alignment, 0);

            result.insert(result.end(), buffer.begin(), buffer.end());
        } else {
            const uint64_t i = std::get<uint64_t>(p.second);

            auto name_length = p.first.size() + 1;
            auto total_size = struct_size + name_length + sizeof(uint64_t);
            auto alignment = (8 - (total_size % 8)) % 8;

            std::vector<uint8_t> buffer;
            buffer.reserve(total_size + alignment);

            IPC_Object_Property prop = {
                .length = total_size + alignment,
                .type = PROPERTY_TYPE_INTEGER,
                .data_start = struct_size + name_length + alignment,
            };
            auto ptr = reinterpret_cast<const uint8_t *>(&prop);
            buffer.insert(buffer.end(), ptr, ptr + sizeof(prop));
            auto name_ptr = reinterpret_cast<const uint8_t *>(p.first.c_str());
            buffer.insert(buffer.end(), name_ptr, name_ptr + name_length);
            buffer.insert(buffer.end(), alignment, 0);
            auto val_ptr = reinterpret_cast<const uint8_t *>(&i);
            buffer.insert(buffer.end(), val_ptr, val_ptr + sizeof(uint64_t));

            result.insert(result.end(), buffer.begin(), buffer.end());
        }
    }
    return result;
}

inline std::vector<uint8_t> BUSObject::serialize_into_ipc(pmos_port_t reply_port, uint64_t user_arg, pmos_port_t handle_port, uint64_t task_group)
{
    auto struct_size = sizeof(IPC_BUS_Publish_Object) - sizeof(IPC_Bus_Object);
    
    auto object = serialize(handle_port, task_group);
    std::vector<uint8_t> data;
    data.reserve(struct_size + object.size());
    IPC_BUS_Publish_Object p = {
        .type = IPC_BUS_Publish_Object_NUM,
        .flags = 0,
        .reply_port = reply_port,
        .user_arg = user_arg,
        .object = {},
    };
    auto ptr = reinterpret_cast<uint8_t *>(&p);
    data.insert(data.end(), ptr, ptr + struct_size);
    data.insert(data.end(), object.begin(), object.end());
    return data;
}

inline std::vector<uint8_t> BUSObject::serialize(pmos_port_t handle_port, uint64_t task_group)
{
    auto name_size_aligned = name.size() + 1;
    name_size_aligned = (name_size_aligned + 7) & ~7;

    auto properties = serialize_properties();
    IPC_Bus_Object object = {
        .size = sizeof(object) + properties.size() + name_size_aligned,
        .name_length = name.size(),
        .properties_offset = name_size_aligned + sizeof(object),
        .handle_port = handle_port,
        .task_group = task_group,
    };

    std::vector<uint8_t> vec;
    vec.reserve(sizeof(object) + properties.size() + name_size_aligned);

    auto object_ptr = reinterpret_cast<uint8_t*>(&object);
    vec.insert(vec.end(), object_ptr, object_ptr + sizeof(object));

    vec.insert(vec.end(), name.begin(), name.end());
    vec.push_back('\0');
    vec.insert(vec.end(), name_size_aligned - name.size() - 1, 0);

    vec.insert(vec.end(), properties.begin(), properties.end());

    return vec;
}

} // namespace pmos::ipc