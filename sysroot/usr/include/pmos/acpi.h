#ifndef _PMOS_ACPI_H
#define _PMOS_ACPI_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct ACPI_RSDP_legacy_descriptor {
 char signature[8];
 uint8_t checksum;
 char OEMID[6];
 uint8_t revision;
 uint32_t rsdt_address;
} __attribute__ ((packed)) ACPI_RSDP_legacy_descriptor;

typedef struct ACPI_RSDP_descriptor {
 struct ACPI_RSDP_legacy_descriptor firstPart;
 
 uint32_t length;
 uint64_t xsdt_address;
 uint8_t extended_checksum;
 uint8_t reserved[3];
} __attribute__ ((packed)) ACPI_RSDP_descriptor;

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif