#include "ubsan.hh"
#include <kern_logger/kern_logger.hh>
#include <stdlib.h>
#define is_aligned(value, alignment) !(value & (alignment - 1))

#define VALUE_LENGTH 40

const char *Type_Check_Kinds[] = {
    "load of",
    "store to",
    "reference binding to",
    "member access within",
    "member call on",
    "constructor call on",
    "downcast of",
    "downcast of",
    "upcast of",
    "cast to virtual base of",
};

static void log_location(source_location *location, Logger& logger) {
    logger.printf("\tfile: %s\n\tline: %i\n\tcolumn: %i\n",
         location->file, location->line, location->column);
}

struct type_mismatch_comm {
    source_location *location;
    type_descriptor *type;
    u64              alignment;
    u8               type_check_kind;
};

void handle_type_mismatch(type_mismatch_comm *type_mismatch, u64 pointer)
{
    source_location *location = type_mismatch->location;
    if (pointer == 0) {
        bochs_logger.printf("Null pointer access\n");
    } else if (type_mismatch->alignment != 0 &&
               is_aligned(pointer, type_mismatch->alignment)) {
        // Most useful on architectures with stricter memory alignment requirements, like ARM.
        bochs_logger.printf("Unaligned memory access\n");
    } else {
        bochs_logger.printf("Insufficient size\n");
        bochs_logger.printf("%s address %p with insufficient space for object of type %s\n",
             Type_Check_Kinds[type_mismatch->type_check_kind], (void *)pointer,
             type_mismatch->type->name);
    }
    log_location(location, bochs_logger);
 
    abort();
}

extern "C" void __ubsan_handle_pointer_overflow(overflow_data *desc, u64 before, u64 after)
{
    source_location *location = &desc->location;

    bochs_logger.printf("Pointer overflow\n");
    bochs_logger.printf("before 0x%x after 0x%x", before, after);
    log_location(location, bochs_logger);
    abort();
}

// extern "C" void __ubsan_handle_shift_out_of_bounds(struct ubsan_shift_desc *desc, uint64_t lhs, uint64_t rhs)
// {

// }

extern "C" void __ubsan_handle_type_mismatch(type_mismatch_info *type_mismatch, u64 pointer) {
    type_mismatch_comm m {
        &type_mismatch->location,
        type_mismatch->type,
        type_mismatch->alignment,
        type_mismatch->type_check_kind
    };

    handle_type_mismatch(&m, pointer);
}
 
extern "C" void __ubsan_handle_type_mismatch_v1(type_mismatch_data_v1 *type_mismatch, u64 pointer) {
    type_mismatch_comm m {
        &type_mismatch->location,
        type_mismatch->type,
        1UL << type_mismatch->log_alignment,
        type_mismatch->type_check_kind
    };

    handle_type_mismatch(&m, pointer);
}


