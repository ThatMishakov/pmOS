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

#ifndef PMOS_LOAD_DATA_H
#define PMOS_LOAD_DATA_H
#include <stdint.h>

enum {
    LOAD_TAG_CLOSE            = 0,
    LOAD_TAG_ARGUMENTS        = 1,
    LOAD_TAG_ENVIRONMENT      = 2,
    LOAD_TAG_STACK_DESCRIPTOR = 3,
    // LOAD_TAG_FILE_DESCRIPTORS = 4,
    LOAD_TAG_LOAD_MODULES     = 5,
    LOAD_TAG_FRAMEBUFFER      = 6,
    LOAD_TAG_RSDP             = 7,
    LOAD_TAG_FDT              = 8,
};

/// @brief Header for a load tag
///
/// These tags are used to pass stdlib-related data to the process upon initialization
/// from the process that spawned it, since the kernel doesn't manage this data.
/// All tags are aligned to 16 bytes (on x86_64) and have this header at the beginning.
/// This list is mapped to the process's address space by the caller and the address
/// and size is passed as the first and second arguments to the process.
struct load_tag_generic {
    uint32_t tag;            //< One of the LOAD_TAG_* constants indicating the type of the tag
    uint32_t flags;          //< Flags for the tag
    uint64_t offset_to_next; //< Offset to the next tag starting from the beginning of this one
};

/// @brief Closing tag. Must be the last tag and has no data, with the offset_to_next field set to 0
struct load_tag_close {
    struct load_tag_generic header;
};
#define LOAD_TAG_CLOSE_HEADER                     \
    {                                             \
        LOAD_TAG_CLOSE, 0, sizeof(load_tag_close) \
    }

/// @brief Stack descriptor tag
///
/// This tag is used to pass the stack descriptor to the process
struct load_tag_stack_descriptor {
    struct load_tag_generic header;
    uint64_t stack_top;  //< Top of the stack
    uint64_t stack_size; //< Size of the stack in bytes (including the guard page)
    uint64_t guard_size; //< Size of the guard area in bytes
    uint64_t unused0;    //< Unused, must be 0
};
#define LOAD_TAG_STACK_DESCRIPTOR_HEADER                                       \
    {                                                                          \
        LOAD_TAG_STACK_DESCRIPTOR, 0, sizeof(struct load_tag_stack_descriptor) \
    }

/// @brief Descriptor of a memory object
struct module_descriptor {
    uint64_t memory_object_id; //< ID of the memory object containing the module
    uint64_t size; //< Size of the module in bytes. The size of the memory object is rounded up to
                   //the nearest page from this.
    uint64_t path_offset; //< Offset to the start of the module name from the start of parent
                          //load_tag_modules_descriptor, < pointing to the null-terminated string
                          //containing the path of the module
    uint64_t cmdline_offset; //< Offset to the start of cmdline argument for the module, similar to
                             //path_offset
};

/// @brief Load module tag
///
/// This tag is used to pass the modules loaded by the kernel to the bootstrap process
struct load_tag_load_modules_descriptor {
    struct load_tag_generic header;
    uint64_t modules_count;             //< Number of modules
    struct module_descriptor modules[]; //< List of the modules, modules_count entries
    // Strings of modules go here
};

/// @brief Framebuffer tag
///
/// This tag is used to pass the framebuffer information, provided by the bootloader to the
/// bootstrap process
struct load_tag_framebuffer {
    struct load_tag_generic header;
    uint64_t framebuffer_addr;   //< Physical address of the framebuffer
    uint32_t framebuffer_width;  //< Width of the framebuffer in pixels
    uint32_t framebuffer_height; //< Height of the framebuffer in pixels
    uint32_t framebuffer_pitch;  //< Pitch of the framebuffer in bytes
    uint32_t framebuffer_bpp;    //< Bits per pixel
    uint8_t memory_model;        //< Memory model of the framebuffer
    uint8_t red_mask_size;       //< Size of the red mask in bits
    uint8_t red_mask_shift;      //< Shift of the red mask
    uint8_t green_mask_size;     //< Size of the green mask in bits
    uint8_t green_mask_shift;    //< Shift of the green mask
    uint8_t blue_mask_size;      //< Size of the blue mask in bits
    uint8_t blue_mask_shift;     //< Shift of the blue mask
    uint8_t unused[1];           //< Unused, must be 0
};
#define LOAD_TAG_FRAMEBUFFER_HEADER                                  \
    {                                                                \
        LOAD_TAG_FRAMEBUFFER, 0, sizeof(struct load_tag_framebuffer) \
    }

/// @brief RSDP tag
///
/// This tag is used to pass the ACPI RSDP pointer
struct load_tag_rsdp {
    struct load_tag_generic header;
    uint64_t rsdp; //< Pointer to the RSDP in the physical memory
};
#define LOAD_TAG_RSDP_HEADER                           \
    {                                                  \
        LOAD_TAG_RSDP, 0, sizeof(struct load_tag_rsdp) \
    }

/// @brief FDT tag
///
/// This tag passes the memory object containing the read-only flat device tree, passed by the
/// bootloader
struct load_tag_fdt {
    struct load_tag_generic header;
    /// Memory object containing the flat binary tree
    uint64_t fdt_memory_object;

    /// Start offset inside the memory object
    uint32_t start_offset;

    /// Size of the memory object in bytes
    uint32_t mem_object_size;
};
#define LOAD_TAG_FDT_HEADER                          \
    {                                                \
        LOAD_TAG_FDT, 0, sizeof(struct load_tag_fdt) \
    }

/// @brief Gets the first tag of the specified type
///
/// @param tag The tag type to search for
/// @param load_data The address of the first tag
/// @param load_data_size The size of the load data
/// @return The address of the first tag of the specified type, or NULL if not found
struct load_tag_generic *get_load_tag(uint32_t tag, void *load_data, size_t load_data_size);

#endif // PMOS_LOAD_DATA_H