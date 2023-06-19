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

#endif // HEADER_H