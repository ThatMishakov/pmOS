/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

#define MADT_LOCAL_APIC_NMI_TYPE 4
typedef struct MADT_LAPIC_NMI_entry {
  MADT_entry header;
  uint8_t ACPI_CPU_UID;
  uint16_t Flags;
  uint8_t LINT_ID;
} __attribute__ ((packed)) MADT_LAPIC_NMI_entry;

#define MADT_RINTC_ENTRY_TYPE 0x18
typedef struct MADT_RINTC_entry {
  MADT_entry header;
  uint8_t version;
  uint8_t reserved;
  uint32_t flags;
  uint64_t hart_id;
  uint32_t acpi_processor_id;
  uint32_t external_interrupt_controller_id;
  uint64_t imsic_base_id;
  uint32_t imsic_size;
} __attribute__ ((packed)) MADT_RINTC_entry;

#define MADT_IMSICT_ENTRY_TYPE 0x19
typedef struct MADT_IMSICT_entry {
  MADT_entry header;
  uint8_t version;
  uint8_t reserved;
  uint32_t flags;
  uint16_t supervisor_mode_interrupt_identities;
  uint16_t guest_mode_interrupt_identities;
  uint8_t guest_index_bits;
  uint8_t hart_index_bits;
  uint8_t group_index_bits;
  uint8_t group_index_shift;
} __attribute__ ((packed)) MADT_IMSICT_entry;

#define MADT_APLIC_ENTRY_TYPE 0x1A
typedef struct MADT_APLIC_entry {
  MADT_entry header;
  uint8_t version;
  uint8_t aplic_id;
  uint32_t flags;
  uint64_t hardware_id;
  uint16_t number_of_idcs;
  uint16_t total_ext_int_sources_supported;
  uint32_t global_system_interrupt_base;
  uint64_t aplic_address;
  uint32_t aplic_size;
} __attribute__ ((packed)) MADT_APLIC_entry;

#define MADT_PLIC_ENTRY_TYPE 0x1B
typedef struct MADT_PLIC_entry {
  MADT_entry header;
  uint8_t version;
  uint8_t plic_id;
  uint64_t hardware_id;
  uint16_t total_ext_int_sources_supported;
  uint16_t max_priority;
  uint32_t flags;
  uint32_t plic_size;
  uint64_t plic_address;
  uint32_t global_sys_int_vec_base;
} __attribute__ ((packed)) MADT_PLIC_entry;

typedef struct MADT {
  ACPISDTHeader header;
  uint32_t local_apic_addr;
  uint32_t flags;
  MADT_entry entries[0];
} __attribute__ ((packed)) MADT;

void init_acpi();

extern RSDP_descriptor20* rsdp_desc;

typedef struct GenericAddressStructure
{
  uint8_t AddressSpace;
  uint8_t BitWidth;
  uint8_t BitOffset;
  // 0 - legacy
  // 1 - byte
  // 2 - word
  // 3 - dword
  // 4 - qword
  uint8_t AccessSize;
  uint64_t Address;
} __attribute__ ((packed)) GenericAddressStructure;
#define ACPI_GAS GenericAddressStructure

ACPISDTHeader* get_table(const char* signature, int n);

typedef struct FADT {
  ACPISDTHeader h;
  uint32_t FIRMWARE_CTRL;
  uint32_t dsdt_phys;
  uint8_t reserved;
  uint8_t preferred_PM_profile;
  uint16_t SCI_INT;
  uint32_t SCI_CMD;
  uint8_t ACPI_ENABLE;
  uint8_t ACPI_DISABLE;
  uint8_t S4BIOS_REQ;
  uint8_t PSTATE_CNT;
  uint32_t PM1a_EVT_BLK;
  uint32_t PM1b_EVT_BLK;
  uint32_t PM1a_CNT_BLK;
  uint32_t PM1b_CNT_BLK;
  uint32_t PM2_CNT_BLK;
  uint32_t PM_TMR_BLK;
  uint32_t GPE0_BLK;
  uint32_t GPE1_BLK;
  uint8_t PM1_EVT_LEN;
  uint8_t PM1_CNT_LEN;
  uint8_t PM2_CNT_LEN;
  uint8_t PM_TMR_LEN;
  uint8_t GPE0_BLK_LEN;
  uint8_t GPE1_BLK_LEN;
  uint8_t GPE1_BASE;
  uint8_t CST_CNT;
  uint16_t P_LVL2_LAT;
  uint16_t P_LVL3_LAT;
  uint16_t FLUSH_SIZE;
  uint16_t FLUSH_STRIDE;
  uint8_t DUTY_OFFSET;
  uint8_t DUTY_WIDTH;
  uint8_t DAY_ALRM;
  uint8_t MON_ALRM;
  uint8_t CENTURY;

  uint16_t IAPC_BOOT_ARCH;
  uint8_t reserved_1;
  
  uint32_t Flags;
  GenericAddressStructure RESET_REG;
  uint8_t RESET_VALUE;
  uint16_t ARM_BOOT_ARCH;
  uint8_t FADT_Minor_Version;
  
  uint64_t X_FIRMWARE_CTRL;
  uint64_t X_DSDT;
  
  GenericAddressStructure X_PM1a_EVT_BLK;
  GenericAddressStructure X_PM1b_EVT_BLK;
  GenericAddressStructure X_PM1a_CNT_BLK;
  GenericAddressStructure X_PM1b_CNT_BLK;
  GenericAddressStructure X_PM2_CNT_BLK;
  GenericAddressStructure X_PM_TMR_BLK;
  GenericAddressStructure X_GPE0_BLK;
  GenericAddressStructure X_GPE1_BLK;
  GenericAddressStructure SLEEP_CONTROL_REG;
  GenericAddressStructure SLEEP_STATUS_REG;
  uint64_t Hypervisor_Vendor_Identity;
} __attribute__ ((packed)) FADT;

typedef struct HPET_Description_Table
{
  ACPISDTHeader h;
  union {
    struct {
      uint8_t REV_ID;
      uint8_t NUM_TIM_CAP: 5;
      uint8_t COUNT_SIZE_CAP : 1;
      uint8_t Reserved : 1;
      uint8_t LEG_RT_CAP : 1;
      uint16_t VENDOR_ID;
    } bits;
    uint32_t asint;
  } __attribute__((packed)) Event_Timer_Block_ID;
  
  GenericAddressStructure BASE_ADDRESS;

  uint8_t HPET_Number;
  uint16_t Min_Clock_tick;
  uint8_t page_protection;
} __attribute__ ((packed)) HPET_Description_Table;


typedef struct DSDT {
  ACPISDTHeader h;
  uint8_t definitions[0];
}__attribute__ ((packed)) DSDT;

// https://github.com/riscv-non-isa/riscv-acpi

// -------------- RISC-V Hart Capabilities Table (RCHT) --------------
// If 0, timer interrupt can wake up the CPU from suspend/idle states
// If 1, timer can't wake up the CPU
#define RCHT_TIMER_CANNOT_WAKEUP_CPU (1 << 0)

typedef struct RCHT_node {
  uint16_t type; // Node type
  uint16_t length; // Node length. Including the length of the header
  uint16_t revision; // Node revision
} __attribute__ ((packed)) RCHT_node;

#define RHCT_ISA_STRING_NODE 0
typedef struct RCHT_ISA_STRING_node {
  RCHT_node header;
  uint16_t string_length; // Length of the ISA string
  char string[0]; // RISC-V ISA string
  // Padding to align the next node to 2 bytes
} __attribute__ ((packed)) RCHT_ISA_STRING_node;

#define RCHT_CMO_NODE 1
typedef struct RCHT_CMO_node {
  RCHT_node header;
  uint8_t reserved; // Reserved
  uint8_t CBOM_block_size; //< Cache block size for management instructions, defined as power of 2 exponent.
  uint8_t CBOP_block_size; //< Cache block size for prefetch instructions, defined as power of 2 exponent.
  uint8_t CBOZ_block_size; //< Cache block size for zero instructions, defined as power of 2 exponent.
} __attribute__ ((packed)) RCHT_CMO_node;

#define RCHT_MMU_TYPE_SV39 0
#define RCHT_MMU_TYPE_SV48 1
#define RCHT_MMU_TYPE_SV57 2

#define RCHT_MMU_NODE 2
typedef struct RCHT_MMU_node {
  RCHT_node header;
  uint8_t reserved; // Reserved
  uint8_t mmu_type; // MMU type
} __attribute__ ((packed)) RCHT_MMU_node;

#define RCHT_HART_INFO_NODE 65535
typedef struct RCHT_HART_INFO_node {
  RCHT_node header;
  uint16_t offsets_count; //< Number of elements in the Offsets array
  uint32_t acpi_processor_uid; //< ACPI processor UID
  uint32_t offsets[0]; //< Offsets to the RCHT nodes, relative to the start of RHCT
                       //< Each hart should have at least ISA string node
} __attribute__ ((packed)) RCHT_HART_INFO_node;

// RISC-V Hart Capabilities Table 
typedef struct RCHT {
  ACPISDTHeader h;
  uint32_t flags; // RHCT flags
  uint64_t time_base_frequency; // Frequency of the system counter. Same for all harts in the system
  uint32_t rhct_nodes_count; // Number of RCHT nodes
  uint32_t rhct_node_offset; // Offset to the first RCHT node from the start of the RCHT table
} __attribute__ ((packed)) RCHT;


// -------------- MCFG table --------------
//
// https://wiki.osdev.org/PCI_Express is a good explanation

// Base allocation structure
typedef struct MCFGBase {
  uint64_t base_addr; //< Base address of Enhanced configuration mecanism
  uint16_t group_number; //< PCI Segment Group Number
  uint8_t start_bus_number; //< Start bus number decoded by this host bridge
  uint8_t end_bus_number; //< End bus number decoded by this host bridge
  uint8_t reserved[4];
} __attribute__ ((packed)) MCFGBase;

// MCFG Table
typedef struct MCFG {
  ACPISDTHeader h;
  uint64_t reserved;
  struct MCFGBase structs_list[0];
  // There is a base address structure for each PCI segment group that is not hot pluggable
} __attribute__ ((packed)) MCFG;
/// Returns the number of MCFGBase structures in the MCFG table
#define MCFG_list_size(m) ((m->h.length - sizeof(struct MCFG))/sizeof(struct MCFGBase))


// -------------- SPCR table --------------
typedef struct SPCR {
  ACPISDTHeader h;
  uint8_t interface_type;
  uint8_t reserved[3];
  struct ACPI_GAS address;
  uint8_t interrupt_type;
  uint8_t pc_irq;
  uint32_t interrupt;
  uint8_t configured_baud_rate;
  uint8_t parity;
  uint8_t stop_bits;
  uint8_t flow_control;
  uint8_t terminal_type;
  uint8_t language;
  uint16_t pci_device_id;
  uint16_t pci_vendor_id;
  uint8_t pci_bus_number;
  uint8_t pci_device_number;
  uint8_t pci_function_number;
  uint32_t pci_flags;
  uint8_t pci_segment;

  // Revision 3 or higher
  uint32_t uart_clock_frequency;
  uint32_t precise_baud_rate;
  uint16_t namespace_string_length;
  uint16_t namespace_string_offset;
  // NULL-terminated string
  char namespace_string[0];
} __attribute__ ((packed)) SPCR;

extern int acpi_revision;

// Returns -1 on error or ACPI version otherwise 
int walk_acpi_tables();
int check_table(ACPISDTHeader* header);

void init_lai();

#endif