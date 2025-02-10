#include <types.hh>
#include <utils.hh>
#include <processes/tasks.hh>

extern "C" ReturnStr<bool> user_access_page_fault(unsigned access, const char *faulting_addr,
                                                  char *&dst, char *&src, size_t &size)
{
    TaskDescriptor *current_task = get_current_task();

    ReturnStr<bool> result = current_task->page_table->prepare_user_page((void *)faulting_addr, access);
    if (!result.success())
        return result;

    if (!result.val) {
        current_task->atomic_block_by_page((void *)faulting_addr, &current_task->page_table->blocked_tasks);
        return false;
    }

    if (access == Protection::Readable) {
        assert(faulting_addr >= src);
        size_t diff = faulting_addr - src;
        assert(diff < size);
        dst += diff;
        src += diff;
        size += diff;
    } else {
        assert(access == Protection::Writeable);
        assert(faulting_addr >= dst);
        size_t diff = faulting_addr - dst;
        assert(diff < size);
        dst += diff;
        src += diff;
        size += diff;
    }

    return result;
}