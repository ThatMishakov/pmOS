#pragma once
#include <stdint.h>
#include <pmos/system.h>
#include "init/init.h"

void pmbus_callback(int result, const char * right_name, pmos_right_t right);
void hook_match_service(struct Service *service, uint64_t object_id);