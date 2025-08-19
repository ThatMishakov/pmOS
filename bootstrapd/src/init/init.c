#include "init.h"
#include <pmos/vector.h>
#include <stdlib.h>

void free_service(struct Service *service)
{
    if (!service)
        return;

    free(service->name);
    free(service->path);
    free(service->description);
    VECTOR_FREE(service->instances);

    free(service);
}

struct Service *new_service()
{
    return calloc(1, sizeof(struct Service));
}