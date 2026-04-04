#include "partitions.hh"

#include <array>

bool guid_zero(const uint8_t *guid)
{
    for (int i = 0; i < 16; i++) {
        if (guid[i] != 0)
            return false;
    }
    return true;
}

std::string guid_to_string(const uint8_t *guid)
{
    char buffer[37]; // 36 characters + 1 null terminator
    std::snprintf(buffer, sizeof(buffer),
                  "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X", guid[3],
                  guid[2], guid[1], guid[0], // First 4 bytes (little-endian)
                  guid[5], guid[4],          // Next 2 bytes (little-endian)
                  guid[7], guid[6],          // Next 2 bytes (little-endian)
                  guid[8], guid[9],          // Next 2 bytes
                  guid[10], guid[11], guid[12], guid[13], guid[14], guid[15] // Last 6 bytes
    );
    return std::string(buffer);
}

constexpr uint32_t crc32_polynomial = 0xEDB88320;

// Compute a single entry of the CRC32 table
constexpr uint32_t crc32_table_entry(uint32_t index)
{
    uint32_t crc = index;
    for (int i = 0; i < 8; ++i) {
        if (crc & 1) {
            crc = (crc >> 1) ^ crc32_polynomial;
        } else {
            crc >>= 1;
        }
    }
    return crc;
}

// Generate the CRC32 table at compile time
constexpr auto generate_crc32_table()
{
    std::array<uint32_t, 256> table {};
    for (uint32_t i = 0; i < 256; ++i) {
        table[i] = crc32_table_entry(i);
    }
    return table;
}

// Compile-time CRC32 table
constexpr auto crc32_table = generate_crc32_table();

// Compute CRC32 for a block of data
constexpr uint32_t calculate_crc32(const uint8_t *data, size_t length, uint32_t crc = 0xFFFFFFFF)
{
    for (size_t i = 0; i < length; ++i) {
        uint8_t byte          = data[i];
        uint32_t lookup_index = (crc ^ byte) & 0xFF;
        crc                   = (crc >> 8) ^ crc32_table[lookup_index];
    }
    return ~crc; // Final XOR
}

bool verify_gpt_checksum(const GPTHeader &header)
{
    constexpr size_t header_size = 92;
    uint32_t expected_crc        = header.header_crc32;

    // Temporarily zero out the checksum field
    uint8_t gpt_copy[header_size];
    std::memcpy(gpt_copy, &header, header_size);
    std::memset(gpt_copy + 16, 0, 4);

    uint32_t actual_crc = calculate_crc32(gpt_copy, header_size);
    return actual_crc == expected_crc;
}
