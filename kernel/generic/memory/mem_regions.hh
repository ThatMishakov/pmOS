/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#include "pmm.hh"
#include "pmos/containers/intrusive_bst.hh"
#include "rcu.hh"

#include <lib/memory.hh>
#include <lib/string.hh>
#include <types.hh>

namespace kernel
{

namespace ipc
{
    class Port;
};
namespace proc
{
    class TaskDescriptor;
}

namespace paging
{
    class Page_Table;
    struct Page_Info;

    struct Page_Table_Arguments;

    class TLBShootdownContext;

    /**
     * @brief Generic memory region class. Defines an action that needs to be executed when
     * pagefault is produced.
     *
     * This is a polymorphic base for memory regions, which describe different ways of how kernel
     * should map its memory and deal with pagefaults and whatnot. These regions are stored a splay
     * tree in the parent Page_Table class.
     *
     * In the "simple" region, the kernel might "simply" allocate the empty pages or map physical
     * memory to the process, however in a more complex situations, this task is pushed onto the
     * userspace pagers/memory managers/programs which like to interract with kernel too much for no
     * aparent reason, depending on how the region is configured.
     */
    struct Generic_Mem_Region {
        virtual ~Generic_Mem_Region() = default;

        pmos::containers::RBTreeNode<Generic_Mem_Region> bst_head;

        union {
            memory::RCU_Head rcu_head;
        };

        /// Asigns the ID sequentially upon region creation.
        Generic_Mem_Region();

        void rcu_free() noexcept;
        static void rcu_callback(void *head, bool chained);

        /// Page aligned start of the region
        void *start_addr = nullptr;

        constexpr bool operator>(void *addr) const { return start_addr > addr; }
        constexpr bool operator<(void *addr) const { return start_addr < addr; }

        /// Page aligned size
        size_t size = 0;

        /// Optional name of the region
        klib::string name;

        /// Unique ID which identifies the region
        u64 id = 0;

        /// Owner of the region
        Page_Table *owner;

        /// Flags for the region
        u8 access_type = 0;

        static constexpr u8 Readable   = 0x01;
        static constexpr u8 Writeable  = 0x02;
        static constexpr u8 Executable = 0x04;

        /**
         * @brief Allocate a page inside the region
         *
         * This function tries to allocate the page for the task (as the kernel is *very* lazy
         * (which is a good thing) and uses delayed allocation for everything). Depending on the
         * actual region class, different actions could be done, one of them being mercilessly
         * blocking the task desperately trying to access it.
         *
         * @param ptr_addr The address which needs to be allocated/has caused the petition.
         * @return true if the page is avaiable, false otherwise.
         */
        [[nodiscard]] virtual ReturnStr<bool> alloc_page(void *ptr_addr, Page_Info info,
                                                         unsigned access_type) = 0;

        /**
         * @brief Checks if a page from the region can be taken out by provide_page() syscall
         */
        constexpr virtual bool can_takeout_page() const noexcept { return false; }

        /**
         * @brief Prepares the page for being accessed by the kernel.
         *
         * This function prepares the page to be accessed by the kernel by checking permissions and
         * whatnot and calling alloc_page() with the specially-crafter parameters. See alloc_page()
         * for more details.
         *
         * @param access_mode Mode in which the page is to be accessed
         * @param page_addr Address of the page
         * @return true if the execution was successfull and the page is immediately available
         * @return false is the execution was successfull but the resource is currently unavailable
         * and the operation is needed to be repeated again once it becomes available.
         * @see alloc_page()
         */
        [[nodiscard]] ReturnStr<bool> prepare_page(unsigned access_mode, void *page_addr);

        // /**
        //  * @brief Function to be executed upon a page fault.
        //  *
        //  * @param error Pagefault error (passed with the exception)
        //  * @param pagefault_addr Address of the pagefault
        //  * @param task Task causing the pagefault
        //  * @return true Execution was successfull and the page is immediately available
        //  * @return false Execution was successfull but the page is not immediately available
        //  * @todo This function is very x86-specific
        //  */
        // bool on_page_fault(u64 error, u64 pagefault_addr);

        /**
         * @brief Function to be executed upon a page fault.
         *
         * This is a generic function, to be executed on page fault. It checks the access
         * permissions and installs the page as needed.
         * @param access_mode Access mode (OR of Readable, Writeable, Executable)
         * @param fault_addr Address of the pagefault
         * @return true Execution was successfull and the page is immediately available
         * @return false Execution was successfull but the page is not immediately available
         */
        ReturnStr<bool> on_page_fault(unsigned access_mode, void *fault_addr);

        Generic_Mem_Region(void *start_addr, size_t size, klib::string name, Page_Table *owner,
                           unsigned access);

        /**
         * @brief Checks if the address is inside the memory region
         *
         * @param addr Memory address
         * @return true It is inside the region
         * @return false It is outside the region
         */
        bool is_in_range(void *addr) const
        {
            return addr >= start_addr and (char *) addr < (char *)start_addr + size;
        }

        bool has_access(unsigned access_mask) const noexcept
        {
            return not(access_mask & ~this->access_type);
        }

        void *addr_end() const noexcept { return (void *)((char *)start_addr + size); }

        /// @brief Prepares the appropriate Page_Table_Arguments for the region
        virtual Page_Table_Arguments craft_arguments() const = 0;

        constexpr virtual bool is_managed() const noexcept { return false; }

        /**
         * @brief Moves the region to the new page table
         *
         * @param new_table The page table to which the region is to be moved
         * @param base_addr Base address in the new page table
         * @param new_access New access specifier to be had in the new page table
         */
        [[nodiscard]] virtual kresult_t move_to(TLBShootdownContext &ctx,
                                                const klib::shared_ptr<Page_Table> &new_table,
                                                void *base_addr, unsigned new_access);

        /**
         * @brief Clones the region to the new page table
         *
         * @param new_table The page table to which the region is to be cloned
         * @param base_addr Base address in the new page table
         * @param new_access New access specifier to be had in the new page table
         */
        [[nodiscard]] virtual kresult_t clone_to(const klib::shared_ptr<Page_Table> &new_table,
                                                 void *base_addr, unsigned new_access) = 0;

        /**
         * @brief Prepares a region for being deleted
         *
         * This procedure must be called before deleting the memory region. It does not free pages
         * and that must be done by the caller after calling the function.
         *
         */
        virtual void prepare_deletion() noexcept;

        virtual void trim(void *new_start_addr, size_t new_size_bytes) noexcept = 0;

        virtual kresult_t punch_hole(void *hole_start, size_t hole_size_bytes) = 0;
    };

    /**
     * @brief Physical mapping region class.
     *
     * This region is very simple and just maps the physical pages to the virtual address space.
     * Invaluable for drivers since we are a microkernel.
     *
     * @see syscall_create_phys_map_region()
     */
    struct Phys_Mapped_Region final: Generic_Mem_Region {
        // Allocated a new page, pointing to the corresponding physical address.
        virtual ReturnStr<bool> alloc_page(void *ptr_addr, Page_Info info,
                                           unsigned access_type) override;

        u64 phys_addr_start = 0;
        constexpr bool can_takeout_page() const noexcept override { return false; }

        virtual kresult_t clone_to(const klib::shared_ptr<Page_Table> &new_table, void *base_addr,
                                   unsigned new_access) override;

        virtual Page_Table_Arguments craft_arguments() const override;

        // Constructs a region with virtual address starting at *aligned_virt* of size *size*
        // pointing to *aligned_phys*
        Phys_Mapped_Region(void *aligned_virt, size_t size, u64 aligned_phys);

        Phys_Mapped_Region(void *start_addr, size_t size, klib::string name, Page_Table *owner,
                           unsigned access, u64 phys_addr_start)
            : Generic_Mem_Region(start_addr, size, klib::forward<klib::string>(name), owner,
                                 access),
              phys_addr_start(phys_addr_start)
        {
            assert(!((unsigned long)start_addr & (PAGE_SIZE - 1)));
            assert(!(phys_addr_start & (PAGE_SIZE - 1)));
        };

        void trim(void *new_start_addr, size_t new_size_bytes) noexcept override;
        kresult_t punch_hole(void *hole_addr_start, size_t hole_size_bytes) override;
    };

    class Mem_Object;

    /// Memory region which references memory object
    struct Mem_Object_Reference final: Generic_Mem_Region {
        /// Memory object that is referenced by the region
        const klib::shared_ptr<Mem_Object> references;

        /// Offset in bytes, from the start of the memory region to the start of the memory object.
        /// Before this offset, the pages are zeroed
        u64 start_offset_bytes = 0;

        /// Offset in bytes from the start of the memory object
        u64 object_offset_bytes = 0;

        /// Size in bytes copied from the object. After this size, the pages are zeroed
        u64 object_size_bytes = 0;

        /// Indicates whether the pages should be copied on access
        bool cow = false;

        pmos::containers::RBTreeNode<Mem_Object_Reference> object_bst_head;

        Mem_Object_Reference(void *start_addr, size_t size, klib::string name, Page_Table *owner,
                             unsigned access, klib::shared_ptr<Mem_Object> references,
                             u64 object_offset_bytes, bool copy_on_write, u64 start_offset_bytes,
                             u64 object_size_bytes);

        virtual ReturnStr<bool> alloc_page(void *ptr_addr, Page_Info info,
                                           unsigned access_type) override;

        virtual kresult_t move_to(TLBShootdownContext &ctx,
                                  const klib::shared_ptr<Page_Table> &new_table, void *base_addr,
                                  unsigned new_access) override;
        virtual kresult_t clone_to(const klib::shared_ptr<Page_Table> &new_table, void *base_addr,
                                   unsigned new_access) override;

        virtual Page_Table_Arguments craft_arguments() const override;

        /**
         * Returns the end byte of the memory object that is referenced by the region
         */
        inline u64 object_up_to() const noexcept { return start_offset_bytes + size; }

        virtual void prepare_deletion() noexcept override;

        constexpr bool can_takeout_page() const noexcept override { return false; }

        void trim(void *new_start_addr, size_t new_size_bytes) noexcept override;
        kresult_t punch_hole(void *hole_addr_start, size_t hole_size_bytes) override;
    };

}; // namespace paging
}; // namespace kernel
