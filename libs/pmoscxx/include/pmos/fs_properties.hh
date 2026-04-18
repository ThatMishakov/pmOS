#pragma once
#include <pmos/ipc.h>
#include <vector>
#include <string>
#include <variant>
#include <span>

namespace pmos::ipc {

struct FSPropertyType {
    std::string name;
};
struct FSPropertyUUID {
    uint8_t uuid[16];
};
struct FSPropertyLabel {
    std::string label;
};
struct FSPropertyUnknown {
    uint32_t property_id;
    uint32_t property_flags;
    std::vector<uint8_t> data;
};
using FSProperty = std::variant<FSPropertyType, FSPropertyUUID, FSPropertyLabel, FSPropertyUnknown>;

std::vector<FSProperty> parse_fs_properties(std::span<const uint8_t> properties_data);

}
