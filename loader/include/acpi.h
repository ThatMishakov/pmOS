#ifndef ACPI_H
#define ACPI_H
#include <stdint.h>

typedef struct RSDP_descriptor {
 char signature[8];
 uint8_t checksum;
 char OEMID[6];
 uint8_t revision;
 uint32_t rsdt_address;
} __attribute__ ((packed)) RSDP_descriptor;

typedef struct RSDP_descriptor20 {
 struct RSDP_descriptor firstPart;
 
 uint32_t length;
 uint64_t xsdt_address;
 uint8_t extended_checksum;
 uint8_t reserved[3];
} __attribute__ ((packed)) RSDP_descriptor20;

typedef struct ACPISDTHeader {
  char signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char OEMID[6];
  char OEMTableID[8];
  uint32_t OEM_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
} __attribute__ ((packed)) ACPISDTHeader;



typedef struct XSDT {
  struct ACPISDTHeader h;
  uint64_t PointerToOtherSDT[0];
} __attribute__ ((packed)) XSDT;

typedef struct RSDT {
  struct ACPISDTHeader h;
  uint32_t PointerToOtherSDT[0];
}__attribute__ ((packed)) RSDT;

typedef struct MADT_entry {
  uint8_t type;
  uint8_t length;
} __attribute__ ((packed)) MADT_entry;

#define MADT_LAPIC_entry_type    0
typedef struct MADT_LAPIC_entry
{
  MADT_entry header;
  uint8_t cpu_id;
  uint8_t apic_id;
  uint32_t flags;
} __attribute__ ((packed)) MADT_LAPIC_entry;

#define MADT_IOAPIC_entry_type    1
typedef struct MADT_IOAPIC_entry
{
  MADT_entry header;
  uint8_t ioapic_id;
  uint8_t reserved[1];
  uint32_t ioapic_addr;
  uint32_t global_system_interrupt_base;
} __attribute__ ((packed)) MADT_IOAPIC_entry;

#define MADT_INT_OVERRIDE_TYPE    2
typedef struct MADT_INT_entry {
  MADT_entry header;
  uint8_t bus;
  uint8_t source;
  uint32_t global_system_interrupt;
  uint32_t flags;
} __attribute__ ((packed)) MADT_INT_entry;


typedef struct MADT {
  ACPISDTHeader header;
  uint32_t local_apic_addr;
  uint32_t flags;
  MADT_entry entries[0];
} __attribute__ ((packed)) MADT;

void init_acpi(unsigned long multiboot_str);

extern uint64_t rsdp_desc;
extern uint64_t rsdp20_desc;

#endif