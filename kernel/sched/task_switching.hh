#pragma once
#include <processes/tasks.hh>

// Switches to this process
void switch_to_task(const klib::shared_ptr<TaskDescriptor>& task);