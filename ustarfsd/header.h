#ifndef HEADER_H
#define HEADER_H
#include <stdint.h>

typedef struct TARHeader {
    int8_t file_name[100];
    int8_t file_mode[8];
    int8_t owner_id[8];
    int8_t group_id[8];
    int8_t file_size[12];
    int8_t last_mod_time[12];
    int8_t checksum[8];
    int8_t type;
    int8_t name_of_linked_file[100];

    int8_t ustar_indicator[6];
    int8_t ustar_version[2];
    int8_t owner_user_name[32];
    int8_t owner_group_name[32];
    int8_t device_major_number[8];
    int8_t device_minor_number[8];
    int8_t filename_prefix[155];
} TARHeader;

/**
 * @brief Calculates the checksum of a TARHeader structure.
 *
 * The checksum is calculated by treating the checksum field itself as filled with spaces,
 * performing a simple sum of all bytes in the header, and subtracting the sum from the
 * checksum field size.
 *
 * @param header A pointer to a TARHeader structure.
 * @return The calculated checksum value.
 *
 * @note This function assumes that the TARHeader structure is properly initialized and
 *       contains valid data.
 */
int calculate_checksum(const TARHeader *header);

/**
 * @brief Checks whether a TARHeader structure represents an empty header.
 *
 * An empty header is defined as a TARHeader structure where all fields are zeroed.
 *
 * @param header A pointer to a TARHeader structure.
 * @return 1 if the header is empty, 0 otherwise.
 *
 * @note This function assumes that the TARHeader structure is properly initialized and
 *       contains valid data.
 */
int is_empty_header(const TARHeader *header);

#endif // HEADER_H