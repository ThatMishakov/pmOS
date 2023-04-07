#pragma once
#include <lib/memory.hh>
#include <lib/vector.hh>
#include <lib/set.hh>
#include <lib/list.hh>
#include <lib/splay_tree_map.hh>

class Page_Table;
class Generic_Mem_Region;
class Port;

/**
 * @brief Memory object used for shared memory
 * 
 * This class represents a signe shared object that can be allocated and then referenced by the userspace.
 * In my vision, this should provide the necessary abstraction to implement filesystem, all sorts of shared
 * memory (mmap with both MAP_SHARED and MAP_PRIVATE) and provide an efficient way for inter-process data
 * exchange. This objects can then be referenced by the memory regions, which, upon the page fault, will
 * call this structure to request the pages contained in it. Shared memory can then be implemented by
 * directly mapping pages stored in this object and CoW memory can be implemented in a similar manner by
 * copying pages upon the access.
 * 
 */
class Mem_Object: public klib::enable_shared_from_this<Mem_Object> {
public:
    /// Type for identifying memory objectrs
    using id_type = u64;

    /// Gets the id of the memory region
    id_type get_id() const noexcept;

    Spinlock lock;

    /**
     * @brief Creates a new memory object
     * 
     * Creates and prepares a memory object, initializing all the internal structures
     * 
     * @param page_size_log log2 of the size of page
     * @param size_pages Size of the object in the number of pages
     * @return klib::shared_ptr<Mem_Object> newly created object
     */
    static klib::shared_ptr<Mem_Object> create(u64 page_size_log, u64 size_pages);

    /**
     * @brief Gets the memory object by its id. If the object is not found, a null pointer is returned
     * 
     * @param id ID if the memory object to be searched for
     * @return Pinter to the memory object. If the object is not found, null is returned
     */
    static klib::shared_ptr<Mem_Object> get_object_or_null(id_type id);

    /**
     * @brief Get the pointer to the memory object by its ID. Throws if the object was not found
     * 
     * @param id ID of the memory object to be searched for
     * @return Valid pointer to the memory object
     */
    static klib::shared_ptr<Mem_Object> get_object(id_type id);

    /**
     * @brief Returns the size of the region in pages
     * 
     * @return Size of the region in pages
     * @see size_bytes()
     */
    u64 size_pages() const noexcept;

    /**
     * @brief Returns the size of the region in bytes
     * 
     * @return Size of the region in bytes
     */
    u64 size_bytes() const noexcept;

    /**
     * @brief Returns the size of the page in bytes
     * 
     * @return Size of one page referenced by the region in bytes
     */
    u64 page_size_bytes() const noexcept;

    /**
     * @brief Requests the page at the *offset*
     * 
     * Attempts to request a page and returns the result. If the page is immediately available, then the first member is true and page_ptr indicates
     * the pointer to the base of the page at the requested offset. If the page is not immediately available, an attempt to allocate the page is made and
     * false is returned. In this case, the page_ptr shall be ignored and the process requesting the page should be blocked or the error might be returned
     * if the page was needed immediately. Later, when the page becomes allocated, this class will iterate through the regions that are referenced by it and
     * provide the page. When mapping the page, it must later not be deallocated when deleting the region.
     * 
     * @param offset Offset to the start of the page in byte. This function does not do bounds checking
     * @return first - page is immediately available and can be maped. Otherwise, the request to allocate the page has been made and the process should be
     *                 blocked before the page is available.
     *         second - pointer to the start of the page at the offset. If the first is not true, this value is meaningless and should be ignored.
     */
    klib::pair<bool /* available */, u64 /* page_ptr */> request_page(u64 offset);

    /**
     * @brief Atomically resizes the memory region
     * 
     * This function resizes the memory region, either adding new pages or deleting some and notifies the referenced memory regions of the changes.
     * It also deallocates the pages contained if needed. Userspace is responsible to save the memory contents beforehands if memory content is
     * needed to be preserved.
     * 
     * @param new_size_pages New size of the object in pages. 0 is a valid size.
     */
    void atomic_resize(u64 new_size_pages);

    /// Registers a pined by page table 
    void register_pined(klib::weak_ptr<Page_Table> pined_by);
    void atomic_register_pined(klib::weak_ptr<Page_Table> pined_by);

    /// Deletes a pined by page table
    void unregister_pined(const klib::weak_ptr<Page_Table> &pined_by) noexcept;
    void atomic_unregister_pined(const klib::weak_ptr<Page_Table> &pined_by) noexcept;
protected:
    Mem_Object() = delete;

    /// @brief Constructs an object
    /// @param page_size_log log2 of the size of page
    /// @param size_pages Size of the memory in pages. Creates the pages vector of this size
    Mem_Object(u64 page_size_log, u64 size_pages);

    /**
     * @brief Id of the memory region
     * 
     * This id is unique and can be used to reference the memory region. cerate_id() should sequentially allocate new ids.
     * 
     */
    id_type id = create_id();

    /**
     * @brief Asigns a new Memory Object id
     * 
     * @return New id
     */
    static inline id_type create_id() noexcept
    {
        static id_type id = 0;

        return __atomic_add_fetch(&id, 1, 0);
    }

    /**
     * @brief Structrue holding information about pages
     * 
     * This structure holds the necessary information needed to allocate the pages.
     */
    struct Page_Storage {
        bool present : 1 = false;
        bool dont_delete : 1 = false;
        u8 pager_index : 3 = 0;
        u64 ppn : 54 = 0;
    } PACKED ALIGNED(8);

    /**
     * @brief Size of the single page
     * 
     * This number can be used to get the page size by shifting 2 by this number. This allows for supporting different architectures with different
     * page sizes. This also indicates the allignment.
     */
    u8 page_size_log = 12;

    /**
     * @brief Storage for the pages
     * 
     * This structure holds the pages referenced by the structure. page_vec_index() function can be used to get the right index according to the
     * page size.
     */
    klib::vector<Page_Storage> pages;

    /**
     * @brief Finds the index for the pointer
     * 
     * @param ptr Pointer to the start of the page for which the index is needed
     * @return Index to the pages vector
     */
    size_t page_vec_index(u64 ptr) const noexcept
    {
        return ptr >> page_size_log;
    }

    /**
     * @brief Page tables pinning the memory region
     * 
     * This set holds the pointers to the page tables currently pining this region. This mechanism can be used for holding
     * the memory region from deliting if it stops being used by all the processes and it is not desired free all the pages.
     * 
     * The Page_Table is used to pin the objects instead of TaskDescriptor because the kernel treats threads as different tasks
     * that share the page table without specific knowledge about what is actually being executed. Thus, in my vision, Page_Table
     * structure can be used as a representation of UNIX processes.
     * 
     */
    klib::set<klib::weak_ptr<Page_Table>> pined_by;

    /**
     * @brief Shared regions referencing the memory region
     * 
     * This set holds the pointers to the memory regions currently referencing this memory object.
     * 
     */
    klib::set<klib::weak_ptr<Generic_Mem_Region>> referenced_by;

    /**
     * @brief Pager for the region
     * 
     * A pager is used for page faults. Upon an attempt to reference the page, if it is found that the page is not allocated,
     * a message is sent to the port to try and allocate the page. If the pager has not been able to provide the page, an
     * attempt to allocate a zero-filled page will be made.
     */
    klib::weak_ptr<Port> pager;
private:
    /// @brief Global storage of the memory objects accessible by ID
    /// @todo Consider a hash table instead
    static inline klib::splay_tree_map<id_type, klib::weak_ptr<Mem_Object>> objects_storage;

    /// @brief Lock of the map containing all the memory objects
    static inline Spinlock object_storage_lock;

    /**
     * @brief Saves the memory object in the map of the memory object
     * 
     */
    static void atomic_push_global_storage(klib::shared_ptr<Mem_Object> o);

    /**
     * @brief Deletes the memory object from the map of the objects
     * 
     * @param object_to_delete ID of the object that needs to be deleted from the map
     */
    static void atomic_erase_gloabl_storage(id_type object_to_delete);

public:
    /**
     * @brief Destroy the Mem_Object object
     * 
     */
    ~Mem_Object();
};