#include "ata.hh"

std::string IDENTIFYData::get_model() const
{
    char model[41];
    ata_string_to_cstring(model_number, model);
    return std::string(model);
}

std::pair<size_t, size_t> IDENTIFYData::get_sector_size() const
{
    if ((physical_sector_size & (1 << 14)) and !(physical_sector_size & (1 << 15))) {
        auto logical = 512;
        if (physical_sector_size & (1 << 12)) {
            // 1 = Device Logical Sector Longer than 256 Words
            logical = logical_sector_size1;
            logical |= logical_sector_size2 << 16;
            if (logical < 512)
                throw std::runtime_error("Invalid logical sector size");
        }
        
        auto physical = logical * (1 << (physical_sector_size & 0xf));
        return {logical, physical};
    } else {
        return {512, 512};
    }
}

uint64_t IDENTIFYData::get_sector_count() const
{
    if (supports_lba48()) {
        return user_addressable_sectors_48;
    } else {
        return total_sectors;
    }
}

bool IDENTIFYData::supports_lba48() const
{
    return command_set_2 & (1 << 10);
}