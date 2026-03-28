#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <pmos/system.h>
#include "auxvec.h"

result_t load_executable(uint64_t task_id, uint64_t mem_object_id, unsigned flags,
                         void *userspace_tags, size_t userspace_tags_size,
                         const char *argv[], const char *envp[], const struct AuxVecEntry *auxvec_entries[]);