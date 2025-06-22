#pragma once
#include <pmos/vector.h>
#include <stdint.h>

enum State {
    STATE_UNINITIALIZED,
    STATE_RUNNING,
    STATE_ERROR,
};

struct Service;
VECTOR(struct Service) service_vector;

struct Instance {
    uint64_t id;
    struct Service *parent;
    enum State state;
};
VECTOR(struct Instance) instances_vector;

struct MatchFilter {
    char *key;
    union { // NULL-terminated lists
        char **strings;
        uint64_t **ints;
    };

    // blacklist?
};

struct Service {
    char *name;
    char *path;

    enum State state;

    instances_vector instances;

    bool start_on_boot;

};