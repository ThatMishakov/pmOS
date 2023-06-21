#include "header.h"
#include <stddef.h>


int calculate_checksum(const TARHeader *header) {
    int checksum = 0;
    const int8_t *bytes = (const int8_t *)header;

    // Sum all bytes of the header
    for (size_t i = 0; i < sizeof(TARHeader); ++i) {
        // Exclude checksum field when calculating the checksum
        if (i >= offsetof(TARHeader, checksum) && i < offsetof(TARHeader, type))
            checksum += ' ';
        else
            checksum += bytes[i];
    }

    return checksum;
}

int is_empty_header(const TARHeader *header) {
    // Check if all fields in the header are zeroed
    int i;
    for (i = 0; i < sizeof(TARHeader); i++) {
        if (((unsigned char *)header)[i] != 0) {
            return 0; // Not empty
        }
    }
    return 1; // Empty header
}