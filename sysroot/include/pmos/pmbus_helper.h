#pragma once
#include "pmbus_object.h"
#include "helpers.h"

struct pmbus_helper;

/// @brief Creates a new pmbus helper, associated with the given msgloop
/// @param for_msgloop msgloop helper, which will be used to watch for new messages
/// @return new pmbus helper on success, error otherwise
struct pmbus_helper *pmbus_helper_create(struct pmos_msgloop_data *for_msgloop);

/// @brief Frees a given pmbus helper. Note: any pending callbacks are not called
/// by this function
/// @param helper Helper to free
void pmbus_helper_free(struct pmbus_helper *helper);

/// Function that gets called once the object is published (or on error). 0 means success,
/// status = -errno encodes an error
typedef void (*pmbus_helper_callback_t)(int status, uint64_t sequence_number, void *ctx, struct pmbus_helper *);

/// @brief Asynchronously publishes an object with pmbus (once it becomes available). Calls callback on success or error
/// @param helper Helper object
/// @param object_owning pmbus object to be published (must not be NULL)
/// @param object_right Right associated to the object (must not be NULL)
/// @param callback Optional callback (called on success or error)
/// @param callback_ctx Context passed to the given callback
/// @return 0 on success, -errno on error
int pmbus_helper_publish(struct pmbus_helper *helper, pmos_bus_object_t *object_owning, pmos_right_t object_right_owning, pmbus_helper_callback_t callback, void *callback_ctx);
