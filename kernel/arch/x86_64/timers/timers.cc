u32 calculate_timer_ticks(const klib::shared_ptr<TaskDescriptor>& task)
{
    return assign_quantum_on_priority(task->priority)*ticks_per_1_ms;
}