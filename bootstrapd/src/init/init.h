#pragma once
#include <pmos/vector.h>
#include <stdint.h>

enum State {
    STATE_UNINITIALIZED,
    STATE_RUNNING,
    STATE_ERROR,
};

struct Service;
VECTOR_TYPEDEF(struct Service, service_vector_type);
extern service_vector_type service_vector;

struct Instance {
    uint64_t id;
    struct Service *parent;
    enum State state;
};
VECTOR_TYPEDEF(struct Instance, instances_vector);

struct MatchFilter {
    char *key;
    union { // NULL-terminated lists
        char **strings;
        uint64_t **ints;
    };

    // blacklist?
};

enum RunType {
    RUN_MANUAL,
    RUN_ALWAYS_ONCE,
    RUN_FIRST_MATCH_ONCE,
    RUN_FOR_EACH_MATCH,
    RUN_UNKNOWN,
};

enum RunType parse_run_type(const char *str);

struct Service {
    char *name;
    char *path;
    char *description;

    enum State state;
    enum RunType run_type;

    instances_vector instances;

    bool start_on_boot;
};

struct Service *new_service();
void free_service(struct Service *service);

void parse_service(const char *cmdline, const char *name, struct Service **out_service);