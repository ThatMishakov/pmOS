#pragma once

/**
 * @brief Temp_Mapper for x86_64 CPUs
 * 
 */
class x86_PAE_Temp_Mapper: public Temp_Mapper {
public:
    virtual void * kern_map(u64 phys_frame) override;
    virtual void return_map(void *) override;

    constexpr x86_PAE_Temp_Mapper() = default;
    x86_PAE_Temp_Mapper(void *virt_addr, u64 cr3);
private:
    PTE * pt_mapped = nullptr;
    unsigned max_index = 0;
    unsigned start_index = 0;
    unsigned min_index   = 1;
    constexpr static unsigned size = 16;
};