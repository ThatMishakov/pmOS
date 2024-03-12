#pragma once
#include <types.hh>
#include "kernel/memory.h"

/**
 * @brief Per-CPU temporary mapper
 * 
 * This class helps with temporary mappings which are needed to access paging structures, which reside scattered in the memory. I have originally used recursive
 * mappings, but I have found them to be really anoying and needing a lot of TLB flushed, si I believe it's a better solution that kills two birds with one stone:
 * I can also access physical memory with it (e.g. for clearing newly allocated pages) without worying about protection violations in the corner cases.
 * 
 * An alternative that Linux is using is linear mapping, however I think it is really ugly and a. consumes precious memory (although who am I kidding) and b.
 * leads to kernel or CPU vulnerabilities being even worse that they already are.
 * 
 * This class is to be stored in a per-CPU CPU_Info structure and allows lock-free temporary mappings of the physical pages into the kernel's virtual space. This
 * is perfect for paging structures and is actually very nice to use.
 * 
 * Upon initialization, we take out 16 pages from the region designated for it and, after preparing the multilevel paging structures and whatnot, map
 * the page directory into the first entry. This leaves us with 15 other entries which can be used for quickly mapping physical memory.
 */
class Temp_Mapper {
public:
    /// @brief  Maps the frame to the kernel virtual address space.
    /// @param phys_frame Physical address of the page
    /// @return Returns the new virtual address or nullpts if the page if there are no slots where the page can be mapped. The latter should never happen
    ///         and if it does, there is a serious bug in the kernel
    virtual void * kern_map(u64 phys_frame) = 0;
    
    /// @brief Returns the mapping back, automatically invalidating the TLB entry
    /// @param virt_addr Virtual address previously returned by kern_map.
    virtual void return_map(void * virt_addr) = 0;

    // TODO: Remap would be nice

    Temp_Mapper() = default;
    Temp_Mapper(const Temp_Mapper&) = delete;
};

/**
 * @brief Request the appropriate temp mapper
 * 
 * This function returns the appropriate temp mapper, local to the CPU if needed.
 * 
 * This is done as a separate function, since initializing CPU-local data (where the temp mapper is allocated
 * on each CPU/hardware thread) requires malloc, which creates a circular dependency.
 * To break the loop, a global static temp mapper is used when bringing up the system and this function
 * provides an interface, which abstracts it away.
*/
Temp_Mapper &request_temp_mapper();

/**
 * @brief Wrapper for Temp_Mapper. Manages mapping and unmapping with trendy RAII, preventing you from forgeting to return the mappings
 * 
 * @tparam P the pointer type stored in the structure.
 */
template<typename P = void>
struct Temp_Mapper_Obj {
    P * ptr = nullptr;
    Temp_Mapper& parent;

    /**
     * @brief Maps the phys_frame to ptr
     * 
     * This function maps the phys_frame to some virtual address, modifying ptr. If ptr was pointing to another mapping, unmaps it first.
     * 
     * @param phys_frame Physical page-aligned address that is to be mapped
     * @return P* new virtual address
     */
    P * map(u64 phys_frame)
    {
        if (ptr != nullptr)
            parent.return_map(reinterpret_cast<void *>(ptr));

        ptr = reinterpret_cast<P *>(parent.kern_map(phys_frame));
        return ptr;
    }

    /**
     * @brief Unmaps the entry
     * 
     * This function returns the map if there was any.
     */
    void clear()
    {
        if (ptr != nullptr) {
            parent.return_map(reinterpret_cast<void *>(ptr));
            ptr = nullptr;
        }
    }


    /**
     * @brief Destroy the Temp_Mapper_Obj object
     * 
     * Upon the object destruction, if it held the mapping, it is returned to the Temp_Mapper.
     */
    ~Temp_Mapper_Obj()
    {
        clear();
    }

    constexpr Temp_Mapper_Obj(Temp_Mapper& t): ptr(nullptr), parent(t) {};

    Temp_Mapper_Obj(Temp_Mapper_Obj&& p): ptr(p.ptr), parent(p.parent)
    {
        p.ptr = nullptr;
    }

    Temp_Mapper_Obj(const Temp_Mapper_Obj&) = delete;
};

/**
 * @brief Temp_Mapper using direct mapping
 * 
 * This class allows use direct physical mapping, typically provided by a bootloader for accessing physical memory
*/
class Direct_Mapper final: public Temp_Mapper {
public:
    virtual void * kern_map(u64 phys_frame) override;
    virtual void return_map(void *) override;

    /**
     * Virtual offset of the direct mapping
     * 
     * This offset is added to the physical address to obtain the virtual address
    */
    u64 virt_offset = 0;
};

// nullptr indicates that the per-CPU mapper must be used
extern Temp_Mapper *global_temp_mapper;