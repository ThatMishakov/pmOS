#include <pmos/ipc/bus_object.hh>

template<class... Ts>
struct overloads : Ts... { using Ts::operator()...; };

namespace pmos::ipc
{
    
void BUSObject::set_name(std::string name)
{
    std::swap(name, this->name);
}

void BUSObject::set_property(std::string_view name, std::variant<std::string_view, uint64_t> p)
{
    const auto visitor = overloads
    {
        [](std::string_view s) -> property { return std::string(s); },
        [](uint64_t i) -> property { return i;} ,
    };
    properties[std::string(name)] = std::visit(visitor, p);
}

std::vector<uint8_t> BUSObject::serialize_properties()
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
                .length = static_cast<uint16_t>(total_size + alignment),
                .type = PROPERTY_TYPE_STRING,
                .data_start = static_cast<uint8_t>(struct_size + name_length),
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
                .length = static_cast<uint16_t>(total_size + alignment),
                .type = PROPERTY_TYPE_INTEGER,
                .data_start = static_cast<uint8_t>(struct_size + name_length + alignment),
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

std::vector<uint8_t> BUSObject::serialize_into_ipc()
{
    auto struct_size = sizeof(IPC_BUS_Publish_Object) - sizeof(IPC_Bus_Object);
    
    auto object = serialize();
    std::vector<uint8_t> data;
    data.reserve(struct_size + object.size());
    IPC_BUS_Publish_Object p = {
        .type = IPC_BUS_Publish_Object_NUM,
        .flags = 0,
        .object = {},
    };
    auto ptr = reinterpret_cast<uint8_t *>(&p);
    data.insert(data.end(), ptr, ptr + struct_size);
    data.insert(data.end(), object.begin(), object.end());
    return data;
}

std::vector<uint8_t> BUSObject::serialize()
{
    auto name_size_aligned = name.size() + 1;
    name_size_aligned = (name_size_aligned + 7) & ~7;

    auto properties = serialize_properties();
    IPC_Bus_Object object = {
        .size = static_cast<uint32_t>(sizeof(object) + properties.size() + name_size_aligned),
        .name_length = static_cast<uint16_t>(name.size()),
        .properties_offset = static_cast<uint16_t>(name_size_aligned + sizeof(object)),
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

const std::string &BUSObject::get_name() const
{
    return name;
}

auto alignup(size_t value, size_t alignment) -> size_t
{
    return (value + alignment - 1) & ~(alignment - 1);
}

struct EqualsFilterBinary {
    uint32_t type;
    uint32_t total_size; // Size of the key and value strings, aligned to 8 bytes
    uint32_t key_len;
    uint32_t value_len;
    // Followed by key and value strings, aligned to 8 bytes
};

struct ConDisFilterBinary {
    uint32_t type;
    uint32_t total_size; // Size of the inner filters, aligned to 8 bytes
    // Followed by inner filters
};

size_t filter_serialized_size(const AnyFilter &filter)
{
    const auto visitor = overloads
    {
        [](const EqualsFilter &f) -> size_t {
            auto size = sizeof(EqualsFilterBinary) + f.name().size() + 1 + f.value().size() + 1;
            return alignup(size, 8);
        },
        [](const Conjunction &f) -> size_t {
            size_t size = sizeof(ConDisFilterBinary);
            for (const auto &operand: f.operands())
                size += filter_serialized_size(operand);
            return size;
        },
        [](const Disjunction &f) -> size_t {
            size_t size = sizeof(ConDisFilterBinary);
            for (const auto &operand: f.operands())
                size += filter_serialized_size(operand);
            return size;
        },
        [](const NoFilter &) -> size_t {
            return 0;
        },
    };
    return std::visit(visitor, filter);
}

constexpr uint32_t PMOS_BUS_FILTER_EQUALS_TYPE = 1;
constexpr uint32_t PMOS_BUS_FILTER_CONJUNCTION_TYPE = 2;
constexpr uint32_t PMOS_BUS_FILTER_DISJUNCTION_TYPE = 3;

static void filter_serialize_push_back(std::vector<uint8_t> &vec, const AnyFilter &filter)
{
    const auto visitor = overloads
    {
        [&](const EqualsFilter &f) {
            uint32_t total_size = sizeof(EqualsFilterBinary) + f.name().size() + 1 + f.value().size() + 1;
            total_size = alignup(total_size, 8);
            vec.reserve(vec.size() + filter_serialized_size(f));
            EqualsFilterBinary binary = {
                .type = PMOS_BUS_FILTER_EQUALS_TYPE,
                .total_size = total_size,
                .key_len = static_cast<uint32_t>(f.name().size()),
                .value_len = static_cast<uint32_t>(f.value().size()),
            };

            auto ptr = reinterpret_cast<const uint8_t *>(&binary);
            vec.insert(vec.end(), ptr, ptr + sizeof(binary));
            auto name_ptr = reinterpret_cast<const uint8_t *>(f.name().c_str());
            vec.insert(vec.end(), name_ptr, name_ptr + f.name().size() + 1);
            auto value_ptr = reinterpret_cast<const uint8_t *>(f.value().c_str());
            vec.insert(vec.end(), value_ptr, value_ptr + f.value().size() + 1);
            vec.insert(vec.end(), total_size - (sizeof(binary) + binary.key_len + 1 + binary.value_len + 1), 0);
        },
        [&](const Conjunction &f) {
            uint32_t total_size = filter_serialized_size(f);
            vec.reserve(vec.size() + total_size);
            ConDisFilterBinary binary = {
                .type = PMOS_BUS_FILTER_CONJUNCTION_TYPE,
                .total_size = total_size,
            };
            auto ptr = reinterpret_cast<const uint8_t *>(&binary);
            vec.insert(vec.end(), ptr, ptr + sizeof(binary));
            for (const auto &operand: f.operands())
                filter_serialize_push_back(vec, operand);
        },
        [&](const Disjunction &f) {
            uint32_t total_size = filter_serialized_size(f);
            vec.reserve(vec.size() + total_size);
            ConDisFilterBinary binary = {
                .type = PMOS_BUS_FILTER_DISJUNCTION_TYPE,
                .total_size = total_size,
            };
            auto ptr = reinterpret_cast<const uint8_t *>(&binary);
            vec.insert(vec.end(), ptr, ptr + sizeof(binary));
            for (const auto &operand: f.operands())
                filter_serialize_push_back(vec, operand);
        },
        [&](const NoFilter &) {
            // No data to serialize
        },
    };
    std::visit(visitor, filter);
}

std::vector<uint8_t> serialize_filter_ipc(const AnyFilter &filter, uint64_t from_sequence_number)
{
    std::vector<uint8_t> result;

    IPC_BUS_Request_Object request = {
        .type = IPC_BUS_Request_Object_NUM,
        .flags = 0,
        .start_sequence_number = from_sequence_number,
    };
    auto request_ptr = reinterpret_cast<uint8_t *>(&request);
    result.insert(result.end(), request_ptr, request_ptr + sizeof(request));

    filter_serialize_push_back(result, filter);

    return result;
}

}