#pragma once
#include <cstdint>

static const uint8_t FIS_TYPE_REG_H2D = 0x27;
struct FIS_Host_To_Device {
    uint8_t fis_type {};
    uint8_t pmport : 4 {};
    uint8_t rsv0 : 2 {};
    uint8_t c : 1 {};
    uint8_t i : 1 {};
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
    uint8_t rsv0 : 1;
    uint8_t d : 1;
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