#pragma once
#include <processes/tasks.hh>

extern "C" void task_switch();

// Switches to this process
void switch_to_task(const klib::shared_ptr<TaskDescriptor>& task);