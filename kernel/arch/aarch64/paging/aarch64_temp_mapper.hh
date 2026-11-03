#pragma once

#include <memory/temp_mapper.hh>

namespace kernel::aarch64::paging
{

class AArch64_Temp_Mapper final: public kernel::paging::Temp_Mapper
{
public:
    virtual void *kern_map(u64 phys_frame) override;
    virtual void return_map(void *) override;

    constexpr AArch64_Temp_Mapper() = default;
    AArch64_Temp_Mapper(void *virt_addr, u64 pt_ptr);

private:
    u64 *pt_mapped         = nullptr;
    unsigned max_index             = 0;
    unsigned start_index           = 0;
    unsigned min_index             = 1;
    constexpr static unsigned size = 16;
};

} // namespace kernel::aarch64::paging