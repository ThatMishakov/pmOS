#include <pmos/fs_properties.hh>
#include <pmos/ipc.h>
#include <stdexcept>

namespace pmos::ipc {

std::vector<FSProperty> parse_fs_properties(std::span<const uint8_t> properties_data)
{
    std::vector<FSProperty> properties;

    size_t offset = 0;
    while (offset < properties_data.size()) {
        if (properties_data.size() - offset < sizeof(IPC_FS_Property))
            throw std::runtime_error("Invalid properties data: not enough data for property header");

        auto *prop_header = reinterpret_cast<const IPC_FS_Property *>(properties_data.data() + offset);
        if (prop_header->property_length < sizeof(IPC_FS_Property) || prop_header->property_length > properties_data.size() - offset)
            throw std::runtime_error("Invalid properties data: invalid property length");

        if (prop_header->data_length > prop_header->property_length - sizeof(IPC_FS_Property))
            throw std::runtime_error("Invalid properties data: data length exceeds property length");

        std::span<const uint8_t> data_span(prop_header->data, prop_header->data_length);
        FSProperty property;
        switch (prop_header->property_id) {
            case IPC_FS_PROPERTY_TYPE: {
                auto type_str = std::string(reinterpret_cast<const char *>(data_span.data()), data_span.size());
                property = FSPropertyType{.name = std::move(type_str)};
                break;
            }
            case IPC_FS_PROPERTY_UUID: {
                if (data_span.size() != 16)
                    throw std::runtime_error("Invalid properties data: UUID property data length must be 16");
                FSPropertyUUID uuid;
                std::copy(data_span.data(), data_span.data() + 16, uuid.uuid);
                property = uuid;
                break;
            }
            case IPC_FS_PROPERTY_LABEL: {
                auto label_str = std::string(reinterpret_cast<const char *>(data_span.data()), data_span.size());
                property = FSPropertyLabel{.label = std::move(label_str)};
                break;
            }
            default: {
                FSPropertyUnknown unknown;
                unknown.property_id = prop_header->property_id;
                unknown.property_flags = prop_header->property_flags;
                unknown.data.assign(data_span.begin(), data_span.end());
                property = std::move(unknown);
                break;
            }
        }
        properties.push_back(std::move(property));
        offset += prop_header->property_length;
    }
    return properties;
}

}