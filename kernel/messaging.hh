#pragma once
#include <stdint.h>
#include "lib/vector.hh"

struct Message {
    uint64_t from;
    uint64_t content;
    uint64_t ptr;
};

using Message_storage = Vector<Message>;