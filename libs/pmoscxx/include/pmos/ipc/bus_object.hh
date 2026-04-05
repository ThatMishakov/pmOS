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
    std::vector<uint8_t> serialize();
    std::vector<uint8_t> serialize_into_ipc();
private:
    std::string name;
    std::unordered_map<std::string, property> properties;

    template<class... Ts>
    struct overloads : Ts... { using Ts::operator()...; };

    std::vector<uint8_t> serialize_properties();
};

} // namespace pmos::ipc