#include <sched/sched.hh>
#include <memory/paging.hh>
#include <processes/tasks.hh>
#include <kern_logger/kern_logger.hh>

extern int kernel_pt_generation;
extern int kernel_pt_active_cpus_count[2];

void park_self()
{
    auto c    = get_cpu_struct();
    auto task = c->current_task;

    task->page_table->unapply_cpu(get_cpu_struct());
    task->before_task_switch();

    // Kill TLB shootdowns
    int kernel_pt_gen = c->kernel_pt_generation;
    __atomic_sub_fetch(&kernel_pt_active_cpus_count[kernel_pt_gen], 1, __ATOMIC_RELEASE);
    c->kernel_pt_generation = -1;

    serial_logger.printf("Parking cpu %x\n", c->lapic_id);

    __atomic_store_n(&c->online, false, __ATOMIC_RELEASE);
    __asm__ volatile("wbinvd");
    __asm__ volatile("hlt");
}

void deactivate_page_table()
{
    auto c = get_cpu_struct();

    int kernel_pt_gen = c->kernel_pt_generation;
    __atomic_sub_fetch(&kernel_pt_active_cpus_count[kernel_pt_gen], 1, __ATOMIC_RELEASE);
    c->kernel_pt_generation = -1;
}