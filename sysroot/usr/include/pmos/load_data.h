#ifndef PMOS_LOAD_DATA_H
#define PMOS_LOAD_DATA_H
#include <stdint.h>

enum {
    LOAD_TAG_CLOSE = 0,
    LOAD_TAG_ARGUMENTS = 1,
    LOAD_TAG_ENVIRONMENT = 2,
    LOAD_TAG_STACK_DESCRIPTOR = 3,
    // LOAD_TAG_FILE_DESCRIPTORS = 4,
};

/// @brief Header for a load tag
///
/// These tags are used to pass stdlib-related data to the process upon initialization
/// from the process that spawned it, since the kernel doesn't manage this data.
/// All tags are aligned to 16 bytes (on x86_64) and have this header at the beginning.
/// This list is mapped to the process's address space by the caller and the address
/// and size is passed as the first and second arguments to the process.
struct load_tag_generic {
    uint32_t tag; //< One of the LOAD_TAG_* constants indicating the type of the tag
    uint32_t flags; //< Flags for the tag
    uint64_t offset_to_next; //< Offset to the next tag starting from the beginning of this one
};

/// @brief Gets the first tag of the specified type
///
/// @param tag The tag type to search for
/// @param load_data The address of the first tag
/// @param load_data_size The size of the load data
/// @return The address of the first tag of the specified type, or NULL if not found
struct load_tag_generic * get_load_tag(uint32_t tag, void * load_data, size_t load_data_size);

/// @brief Closing tag. Must be the last tag and has no data, with the offset_to_next field set to 0
struct load_tag_close {
    struct load_tag_generic header;
};

/// @brief Stack descriptor tag
///
/// This tag is used to pass the stack descriptor to the process
struct load_tag_stack_descriptor {
    struct load_tag_generic header;
    uint64_t stack_top; //< Top of the stack
    uint64_t stack_size; //< Size of the stack in bytes (including the guard page)
    uint64_t guard_size; //< Size of the guard area in bytes
    uint64_t unused0; //< Unused, must be 0
};

#endif // PMOS_LOAD_DATA_H