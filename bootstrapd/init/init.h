#pragma once
#include <pmos/vector.h>

enum RunPolicy {
    POLICY_RUN_ONCE,
    POLICY_RUN_MULTIPLE,
};

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
};
VECTOR(struct Instance) instances_vector;

struct Service {
    char *name;
    char *path;

    enum RunPolicy run_policy;
    enum State state;

    service_vector depends_on;
    instances_vector instances;
};