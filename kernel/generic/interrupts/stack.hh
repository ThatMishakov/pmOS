#pragma once
#include <types.hh>

/// Size of the kernel stacks
static const long STACK_SIZE = KB(32);

/**
 * This structure holds the kernel stack. In pmOS, we use 1 (actually 5) stack(s) per CPU and do task switches by changing the
 * *current task* pointer and not by switching stacks.
 */
struct Stack {
    u8 byte[STACK_SIZE];

    inline u64* get_stack_top()
    {
        return (u64*)&(byte[STACK_SIZE]);
    }
};

class Kernel_Stack_Pointer {
protected:
    klib::unique_ptr<Stack> stack = klib::make_unique<Stack>();
public:
    inline u64* get_stack_top()
    {
        return stack->get_stack_top();
    }
};