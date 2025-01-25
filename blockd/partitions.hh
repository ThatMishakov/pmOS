#pragma once
#include <stdint.h>
#include <string>

struct MBRPartition {
    uint8_t status;
    uint8_t chs_start[3];
    uint8_t type;
    uint8_t chs_end[3];
    uint32_t lba_start;
    uint32_t num_sectors;
} __attribute__((packed));

struct MBR {
    uint8_t boot_code[440];
    uint32_t disk_signature;
    uint16_t reserved;
    MBRPartition partitions[4];
    uint16_t magic;

    static constexpr uint16_t MAGIC = 0xAA55;
};
static_assert(sizeof(MBR) == 512);

struct GPTHeader {
    uint8_t signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t disk_guid[16];
    uint64_t partition_entry_lba;
    uint32_t num_partition_entries;
    uint32_t partition_entry_size;
    uint32_t partition_entry_array_crc32;
} __attribute__((packed));

static_assert(sizeof(GPTHeader) == 92);

struct GPTPartitionEntry {
    uint8_t type_guid[16];
    uint8_t partition_guid[16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t attributes;
    uint16_t name[36];
};

bool verify_gpt_checksum(const GPTHeader &header);
bool guid_zero(const uint8_t *guid);
std::string guid_to_string(const uint8_t *guid);