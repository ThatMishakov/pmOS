#include <processes/tasks.hh>

void TaskDescriptor::before_task_switch() {
    // At the moment, nothing need to be done
    // To be expanded when floating point support is added
}

void TaskDescriptor::after_task_switch() {
}