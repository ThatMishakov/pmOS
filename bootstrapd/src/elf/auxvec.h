#pragma once
#include <stddef.h>
#include <stdint.h>
#include <pmos/vector.h>

enum AuxVecDataType {
    DATA_TYPE_LONG,
    DATA_TYPE_PTR,
    DATA_TYPE_EXTERNAL,
};


/// Auxiliary data (not owned), that can be passed by a_ptr aux vector entry.
/// Will be aligned to 4 or 8 bytes when serializing
struct AuxVecExtData {
    size_t size;
    const void *data;
};

/// Auxvec entry to be serialized.
struct AuxVecEntry {
    int entry_type;
    enum AuxVecDataType data_type;
    union {
        // using uint64_t instead of void * so that 32 bit programs
        // can start 64 bit processes (DATA_TYPE_PTR)
        uint64_t ptr;
        // Same as before (DATA_TYPE_LONG)
        int64_t long_data; 
        // External data, that will be placed in the information block.
        // Can be of any size, will be aligned to 4 or 8.
        struct AuxVecExtData external_data;
    };
};

VECTOR_TYPEDEF(char *, auxvec_vector_cstr_t);
VECTOR_TYPEDEF(struct AuxVecEntry, auxvec_vector_entry_t);
VECTOR_TYPEDEF(struct AuxVecExtraData, auxvec_vector_extra_t);

struct AuxVecBuilder {
    bool ptr_is_64bit;
    // Argc is calculated from this
    auxvec_vector_cstr_t argv;
    auxvec_vector_cstr_t envp;
    auxvec_vector_entry_t entries;
};

/// @brief Size of the data serialized into the ELF format
/// @param builder Pointer to the valid builder
/// @return Size (in bytes) of the data
size_t auxvec_size_serialized(const struct AuxVecBuilder *builder);

/// @brief Serialize the builder into the data, that can be pushed to the stack
///
/// @param builder Pointer to the builder
/// @param stack_top Pointer to the top of the stack of the new process (used as a reference
/// to adjust the pointers to while serializing)
/// @param data_out Pointer to the serialized binary data (should be freed with free)
/// @param size_out Size of the data
/// @return Result. 0 on success, -errno on error
int auxvec_serialize(const struct AuxVecBuilder *builder, uint64_t stack_top, uint8_t **data_out, size_t *size_out);

/// @brief Creates a new builder
///
/// @return The pointer to a new empty builder; NULL on error
struct AuxVecBuilder *auxvec_new();

/// @brief Frees a builder, with all of this data
/// @param builder Pointer to the builder (or NULL)
void auxvec_free(struct AuxVecBuilder *builder);

/// @brief Adds args from the given vector (untill reaching NULL)
/// @param builder Pointer to the builder
/// @param argv Pointer to the arguments (NULL if none)
/// @return Result. 0 on success, -errno on error
int auxvec_push_argv(struct AuxVecBuilder *builder, const char *argv[]);

/// @brief Adds envp from the given vector (untill reaching NULL)
/// @param builder Pointer to the builder
/// @param envp Pointer to the environment pointers (NULL if none)
/// @return Result. 0 on success, -errno on error
int auxvec_push_envp(struct AuxVecBuilder *builder, const char *envp[]);

