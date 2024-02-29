#pragma once

/** Kernel's virtual memory allocator
 * 
 * virtmem is a virtual memory allocator for the kernel. It is inspired by vmem algorithm created by Bonwick and Adams
 * but it currently is a very simple implementation since having a more complete and efficient one is not a priority
 * at the moment.
*/

#include <types.hh>

struct VirtmemBoundaryTag {
    // The fist 2 members must match VirtMemFreelist, to construct a circular linked list.
    // C++ inheritance can't be used because the stadard gives no guarantees of any specific
    // memory layout of derived classes and structs with dynamic data.

    // Either a double linked list of free sigments of a given size
    // or a links of the hash table of allocated segments
    VirtmemBoundaryTag* ll_next = nullptr;
    VirtmemBoundaryTag* ll_prev = nullptr;

    // Base address of the segment
    u64 base = 0;
    // Size of the segment
    u64 size = 0;

    
    // Doubly linked list of all segments, sorted by address, or a doubly linked list of free
    // boundary tags
    VirtmemBoundaryTag* segment_next = nullptr;
    VirtmemBoundaryTag* segment_prev = nullptr;
    // Segment end address
    constexpr u64 end() const { return base + size; }

    enum class State: u8 {
        FREE = 0,
        ALLOCATED,
        LISTHEAD, // The segment is a head of the list of all segments
    } state = State::FREE;
    // Defines if the boundary tag is used
    
    constexpr bool has_prev() const { return segment_prev != nullptr; }
    constexpr bool has_next() const { return segment_next != nullptr; }
};

struct VirtMemFreelist {
    // Circular list is used here since it is supposedly very efficient and easy to implement.
    // This is used for both free segments storage and hash table of allocated segments.

    // Circular doubly linked list of segments, depending on what the list head is used for
    VirtmemBoundaryTag* ll_next = nullptr; // Head
    VirtmemBoundaryTag* ll_prev = nullptr; // Tail

    bool is_empty() const { return ll_next == (VirtmemBoundaryTag*)this; }

    // Point the list to itself, initializing it to the empty state
    // This has to be called manually as in the rellocatable executables, the address is not known
    // at the compile time, so the constructor has to be called. This is problematic, since these
    // lists are used before the global constructors are called.
    void init_empty() {
        ll_next = (VirtmemBoundaryTag*)this;
        ll_prev = (VirtmemBoundaryTag*)this;
    }
};

static const u64 virtmem_initial_segments = 16;
// Static array of initial boundary tags, used during the initialization of the kernel
// This is done this way as the new tags are then allocated using the allocator itself,
// which would not be available during the initialization, creating a chicken-egg problem.
// The next allocations will then work fine, as long as there is 1 free segment in the
// freelist.
inline VirtmemBoundaryTag virtmem_initial_tags[virtmem_initial_segments];

// The unsorted list of unused boundary tags
inline VirtMemFreelist virtmem_available_tags_list;
inline u64 virtmem_available_tags_count = 0;

// Get an unused boundary tag from the list
VirtmemBoundaryTag *virtmem_get_free_tag();
// Return an unused boundary tag to the list
inline void virtmem_return_tag(VirtmemBoundaryTag* tag);

// This function adds the initial tags to the freelist and shall be called during the initialization
// of the allocator during the kernel boot.
void virtmem_fill_initial_tags();

// Page size in bytes
static const u64 freelist_quantum = 12;
// Max log2 of boundary tag size
static const u64 freelist_max_order = 63;
// Number of entries in array of free lists
static const u64 freelist_count = freelist_max_order - freelist_quantum + 1;
// Bitmap indicating which sizes of free lists are non-empty
inline u64 virtmem_freelist_bitmap = 0;

// Array of free lists
inline VirtMemFreelist virtmem_freelists[freelist_count];

// Find the index of the freelists array corresponding to the given size
int virtmem_freelist_index(u64 size);


static const u64 virtmem_initial_hash_size = 16;
// Initial size of the hash table of allocated segments. After the initialization, the hash table
// can then resize itself as needed, switching to different dynamically allocated array.
// The hash table array is always a power of 2, so the mask can be used instead of modulo.
inline VirtMemFreelist virtmem_initial_hash[virtmem_initial_hash_size];

inline u64 virtmem_hashtable_entries = 0;
inline VirtMemFreelist* virtmem_hashtable = nullptr;
inline u64 virtmem_hashtable_size = virtmem_initial_hash_size;
inline u64 virtmem_hashtable_size_bytes() { return virtmem_hashtable_size * sizeof(VirtMemFreelist); }
inline u64 virtmem_hash_mask() { return virtmem_hashtable_size - 1; }

// Save the tag to the hash table. This function will try to optimistically resize the hash table, but
// might postpone it if the system is under memory pressure and the allocation fails.
void virtmem_save_to_alloc_hashtable(VirtmemBoundaryTag* tag);

// Initialize the allocator during booting, and add the space designated for the allocator to the freelist
void virtmem_init(u64 virtmem_base, u64 virtmem_size);


// Adds a boundary tag to a given freelist
void virtmem_add_to_list(VirtMemFreelist* list, VirtmemBoundaryTag* tag);
// Removes a boundary tag from the parent list
void virtmem_remove_from_list(VirtmemBoundaryTag* tag);

// Adds a boundary tag to the appropriate list of free segments
void virtmem_add_to_free_list(VirtmemBoundaryTag* tag);


// Makes sure that enough boundary tags are available
// Returns SUCCESS (0) if the operation was successful or an error code otherwise
u64 virtmem_ensure_tags(u64 size);

enum class VirtmemAllocPolicy {
    INSTANTFIT,
    BESTFIT,
};

// Allocate a segment of a given size, without any specific alignment
// This function does not fill the segment with memory pages, if that is desired, the page
// frame allocator should be invoked separately.
// Returns the base address of the segment or nullptr if the allocation failed (when the system
// is out of memory)
void *virtmem_alloc(u64 npages, VirtmemAllocPolicy policy = VirtmemAllocPolicy::INSTANTFIT);

// Allocate a segment of a given size, with a specific alignment
// This function is similar to virtmem_alloc, but it also takes the alignment into account.
// The alignment is a log2 of the number of pages, so the segment will be aligned to 2^alignment
void *virtmem_alloc_aligned(u64 npages, u64 alignment);

// Free a segment. Only complete segments can be freed and the size is used as a sanity check.
void virtmem_free(void *ptr, u64 npages);

// Links the boundary tag with the list of tags sorted by address
void virtmem_link_tag(VirtmemBoundaryTag* preceeding, VirtmemBoundaryTag* new_tag);

// Unlinks the boundary tag from the list of tags sorted by address
void virtmem_unlink_tag(VirtmemBoundaryTag* tag);

// Dummy head of the list of segments sorted by address
// This is used to simplify the code and avoid special cases when adding new segments
inline VirtmemBoundaryTag segment_ll_dummy_head;