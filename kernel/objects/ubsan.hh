#pragma once
#include <types.hh>

struct source_location {
    const char *file;
    u32 line;
    u32 column;
};
 
struct type_descriptor {
    u16 kind;
    u16 info;
    char name[1];
};
 
struct type_mismatch_info {
    source_location location;
    type_descriptor *type;
    u64 alignment;
    u8 type_check_kind;
};

struct type_mismatch_data_v1 {
	struct source_location location;
	struct type_descriptor *type;
	unsigned char log_alignment;
	unsigned char type_check_kind;
};
 
struct out_of_bounds_info {
    source_location location;
    type_descriptor left_type;
    type_descriptor right_type;
};

struct overflow_data {
	struct source_location location;
	struct type_descriptor *type;
};