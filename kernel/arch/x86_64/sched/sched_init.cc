void init_scheduling()
{
    init_per_cpu();
    
    klib::shared_ptr<TaskDescriptor> current_task = TaskDescriptor::create();
    get_cpu_struct()->current_task = current_task;

    current_task->register_page_table(x86_4level_Page_Table::init_from_phys(getCR3()));
    // Again, there is no reason to store a pointer to the Page_Table instead of x86_Page_Table
    const auto current_pt = klib::dynamic_pointer_cast<x86_Page_Table>(current_task->page_table);
    current_pt->atomic_active_sum(1);

    try {
        current_task->page_table->atomic_create_phys_region(0x1000, GB(4), Page_Table::Readable | Page_Table::Writeable | Page_Table::Executable, true, "init_def_map", 0x1000);

        current_task->pid = pid++;    
        tasks_map.insert({current_task->pid, klib::move(current_task)});
    } catch (const Kern_Exception& e) {
        t_print_bochs("Error: Could not assign the page table to the first process. Error %i (%s)\n", e.err_code, e.err_message);
        throw;
    }
}