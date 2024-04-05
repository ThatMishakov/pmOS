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