#pragma once
#include <pmos/vector.h>
#include <pmos/pmbus_helper.h>
#include <stdint.h>

enum State {
    STATE_UNINITIALIZED,
    STATE_STARTED,
    STATE_RUNNING,
    STATE_ERROR,
};

struct Service;
VECTOR_TYPEDEF(struct Service, service_vector_type);
extern service_vector_type service_vector;

struct Instance {
    uint64_t id;
    uint64_t group_id;
    struct Service *parent;
    enum State state;
};
VECTOR_TYPEDEF(struct Instance, instances_vector);

struct Requirement {
    char *service_name;
    struct Service *service;
};
VECTOR_TYPEDEF(struct Requirement, requirements_vector);

struct PCIFilter {
    // TODO: Add more fields as necessary
    char *class;
    char *subclass;
    char *prog_if;
};

struct MatchFilter {
    char *key;
    union {
        char **strings; // Get converted to ints if necessary
        struct PCIFilter pci_filter;
    };

    bool require_all;
    // blacklist?
};
VECTOR_TYPEDEF(struct MatchFilter, match_filter_vector);

enum RunType {
    RUN_MANUAL,
    RUN_ALWAYS_ONCE,
    RUN_FIRST_MATCH_ONCE,
    RUN_FOR_EACH_MATCH,
    RUN_UNKNOWN,
};

enum RunType parse_run_type(const char *str);

struct module_descriptor_list;

struct Service {
    char *name;
    char *path;
    char *description;

    enum State state;
    enum RunType run_type;

    instances_vector instances;
    requirements_vector requirements;
    match_filter_vector match_filters;

    bool start_on_boot;

    struct HookedService *hook;
    struct PmbusPublishHook *publish_hook;
    struct Service *next;
    struct module_descriptor_list *module;

    uint64_t service_right;
    uint64_t service_recieve_right;
    uint64_t pmbus_id;
};

struct Service *new_service();
void free_service(struct Service *service);

void parse_service(const char *cmdline, const char *name, struct Service **out_service);
void parse_services(struct module_descriptor_list *d);

void publish_service(struct Service *);

void match_services();
void publish_services();

void *construct_filter(struct Service *service);
pmos_bus_object_t *construct_pmbus_object(struct Service *service);


int start_service(struct Service *service, uint64_t object_id, uint64_t optional_right_id);
