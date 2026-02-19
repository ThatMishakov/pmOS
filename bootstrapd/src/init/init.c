#include "init.h"
#include <pmos/vector.h>
#include <stdlib.h>

static void free_match_filters(match_filter_vector filters)
{
    struct MatchFilter f;
    VECTOR_FOREACH(filters, f) {
        free(f.key);
        if (f.strings) {
            char **s = f.strings;
            while (*s) {
                free(*s);
                ++s;
            }
            free(f.strings);
        }
    }
    VECTOR_FREE(filters);
}

void free_service(struct Service *service)
{
    if (!service)
        return;

    free(service->name);
    free(service->path);
    free(service->description);
    VECTOR_FREE(service->instances);

    struct Requirement r;
    VECTOR_FOREACH(service->requirements, r) {
        free(r.service_name);
    }
    VECTOR_FREE(service->requirements);

    free_match_filters(service->match_filters);

    free(service);
}

struct Service *new_service()
{
    return calloc(1, sizeof(struct Service));
}