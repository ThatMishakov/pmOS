#ifndef _PMOS_DWARF_H
#define _PMOS_DWARF_H 1
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct __attribute__((packed)) {
    uint32_t length;
    uint16_t version;
    uint32_t header_length;
    uint8_t min_instruction_length;
    uint8_t default_is_stmt;
    int8_t line_base;
    uint8_t line_range;
    uint8_t opcode_base;
    uint8_t std_opcode_lengths[12];
} DebugLineHeader;

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif