#pragma once
#include <processes/tasks.hh>

// Saves and restores GSBase and FSBase
void save_segments(TaskDescriptor* task);
void restore_segments(TaskDescriptor* task);