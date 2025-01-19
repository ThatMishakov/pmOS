#pragma once
#include <cstdint>
#include <utility>
#include <string>

static const uint8_t FIS_TYPE_REG_H2D = 0x27;
struct FIS_Host_To_Device {
    uint8_t fis_type {};
    uint8_t pmport : 4 {};
    uint8_t rsv0 : 3 {};
    uint8_t c : 1 {};
    uint8_t command {};
    uint8_t featurel {};

    uint8_t lba0 {};
    uint8_t lba1 {};
    uint8_t lba2 {};
    uint8_t device {};

    uint8_t lba3 {};
    uint8_t lba4 {};
    uint8_t lba5 {};
    uint8_t featureh {};

    uint8_t countl {};
    uint8_t count2 {};
    uint8_t icc {};
    uint8_t control {};

    uint8_t rsv1[4] {};
};

static const uint8_t FIS_TYPE_REG_D2H = 0x34;
struct [[gnu::packed]] FIS_Device_To_Host {
    uint8_t fis_type;
    uint8_t pmport : 4;
    uint8_t rsv0 : 2;
    uint8_t i : 1;
    uint8_t rsv1 : 1;
    uint8_t status;
    uint8_t error;

    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t rsv2;

    uint8_t countl;
    uint8_t count2;
    uint8_t rsv3[2];

    uint8_t rsv4[4];
};

static const uint8_t FIS_TYPE_DMA_ACT = 0x39;
// ATA-8 16.5.4 DMA Activate - Device to Host FIS
struct [[gnu::packed]] FIS_DMA_Active {
    uint8_t fis_type;
    uint8_t pmport : 4;
    uint8_t rsv0 : 2;
    uint8_t i : 1;
    uint8_t a : 1;
    uint8_t rsv1[2];
};

static const uint8_t FIS_TYPE_DMA_SETUP = 0x41;
// ATA-8 16.5.5 First Party DMA Setup FIS - Bidirectional
struct [[gnu::packed]] FIS_DMA_Setup {
    uint8_t fis_type;
    uint8_t pmport : 4;
    uint8_t rsv0 : 1;
    uint8_t d : 1;
    uint8_t i : 1;
    uint8_t a : 1;
    uint8_t rsv1[2];

    uint64_t dma_buffer_id;
    uint32_t rsv2;
    uint32_t dma_buffer_offset;
    uint32_t dma_transfer_count;
    uint32_t rsv3;
};

static const uint8_t FIS_TYPE_PIO_SETUP = 0x5F;
// ATA-8 10.5.11.1 PIO Setup FIS - Device to Host FIS
struct [[gnu::packed]] FIS_PIO_Setup {
    uint8_t fis_type;
    uint8_t pmport : 4;
    uint8_t rsv0 : 1;
    uint8_t d : 1;
    uint8_t i : 1;
    uint8_t a : 1;
    uint8_t status;
    uint8_t error;

    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t rsv1;

    uint8_t countl;
    uint8_t counth;
    uint8_t rsv2;
    uint8_t e_status;
    uint16_t transfer_count;
    uint16_t rsv3;
};

static const uint8_t FIS_TYPE_SET_DEV_BITS = 0xA1;
// ATA-8 16.5.3 Set Device Bits - Device to Host FIS
struct [[gnu::packed]] FIS_Set_Device_Bits {
    uint8_t fis_type;
    uint8_t pmport : 4;
    uint8_t rsv0 : 2;
    uint8_t i : 1;
    uint8_t a : 1;
    uint8_t statusl : 3;
    uint8_t rsv1 : 1;
    uint8_t statush : 3;
    uint8_t rsv2 : 1;
    uint8_t error;

    uint32_t reserved;
};

struct [[gnu::packed]] AHCI_FIS {
    FIS_DMA_Setup dsfis;
    uint8_t pad0[4];

    FIS_PIO_Setup psfis;
    uint8_t pad1[12];

    FIS_Device_To_Host rfis;
    uint8_t pad2[4];

    FIS_Set_Device_Bits dbfis;

    uint8_t unknown[64];

    uint8_t rsv[96];
};

struct CommandListEntry {
    uint8_t command_fis_length : 5;
    uint8_t atapi : 1;
    uint8_t write : 1;
    uint8_t prefetchable : 1;

    uint8_t reset : 1;
    uint8_t bist : 1;
    uint8_t clear_busy : 1;
    uint8_t rsv0 : 1;
    uint8_t port_multiplier_port : 4;

    uint16_t prdt_length;

    uint32_t prdb_count;

    uint64_t command_table_base;
    uint32_t rsv1[4];
};

struct PRDT {
    uint64_t data_base {};
    uint32_t rsv0 {};
    uint32_t data_base_count : 22 {};
    uint32_t rsv1 : 9 {};
    uint32_t interrupt_on_completion : 1 {};
};

struct IDENTIFYData {
    uint16_t general_config;
    uint16_t obsolete0[9];
    uint8_t serial_number[20];
    uint16_t obsolete1[3];
    uint8_t firmware_revision[8];
    uint8_t model_number[40];
    uint16_t max_data_transfer;
    uint16_t obsolete2;
    uint16_t capabilities;
    uint16_t capabilities2;
    uint16_t obsolete3[2];
    uint16_t field_validity;
    uint16_t obsolete4[5];
    uint16_t multi_sector;
    uint32_t total_sectors;
    uint16_t obsolete5;
    uint16_t dma_modes;
    uint16_t pio_modes;
    uint16_t min_multiword;
    uint16_t recommended_multiword;
    uint16_t min_pio;
    uint16_t min_pio_nrdy;
    uint16_t rsv0[6];
    uint16_t queue_depth;
    uint16_t sata_capabilities;
    uint16_t sata_features;
    uint16_t rsv1[2];
    uint16_t version_major;
    uint16_t version_minor;
    uint16_t command_set_1;
    uint16_t command_set_2;
    uint16_t command_set_ext;
    uint16_t cfs_enable_1;
    uint16_t cfs_enable_2;
    uint16_t cfs_default;
    uint16_t dma_modes_selected;
    uint16_t secure_erase_time;
    uint16_t enhanced_erase_time;
    uint16_t current_apm;
    uint16_t master_password_id;
    uint16_t hw_reset_result;
    uint16_t acoustic_management;
    uint16_t stream_min_request;
    uint16_t stream_transfer_time;
    uint16_t stream_access_latency;
    uint32_t stream_perf_granularity;
    uint64_t user_addressable_sectors_48;
    uint16_t stream_perf;
    uint16_t rsv2;
    uint16_t physical_sector_size;
    uint16_t interseek_delay;
    uint16_t uuid[4];
    uint16_t world_wide_name[4];
    uint16_t reserved_for_tlc;
    uint16_t logical_sector_size1;
    uint16_t logical_sector_size2;
    uint16_t rsv3[8];
    uint16_t removable_media_status;
    uint16_t security_status;
    uint16_t vendor_specific[31];
    uint16_t cfa_power_mode;
    uint16_t compactflash_reserved[15];
    uint16_t current_media_serial[30];
    uint16_t rsv4[49];
    uint16_t integrity;

    std::pair<size_t, size_t> get_sector_size() const;
    std::string get_model() const;
    bool supports_lba48() const;
    uint64_t get_sector_count() const;
};

template<unsigned N>
void ata_string_to_cstring(const uint8_t (&ata_string)[N], char (&cstring)[N+1]) {
    for (unsigned i = 0; i < N; i += 2) {
        cstring[i]     = ata_string[i + 1];
        cstring[i + 1] = ata_string[i];
    }
    cstring[N] = '\0';
}