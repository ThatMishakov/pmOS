#pragma once
#include <memory/temp_mapper.hh>
#include "riscv64_paging.hh"

/**
 * @brief Temp_Mapper for x86_64 CPUs
 * 
 */
class RISCV64_Temp_Mapper final: public Temp_Mapper {
public:
    virtual void * kern_map(u64 phys_frame) override;
    virtual void return_map(void *) override;

    constexpr RISCV64_Temp_Mapper() = default;
    RISCV64_Temp_Mapper(void *virt_addr, u64 pt_ptr);
private:
    RISCV64_PTE * pt_mapped = nullptr;
    unsigned max_index = 0;
    unsigned start_index = 0;
    unsigned min_index   = 1;
    constexpr static unsigned size = 16;
};

