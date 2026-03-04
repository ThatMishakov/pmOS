#include <pmos/system.h>
#include <pmos/memory.h>
#include <kernel/elf.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <kernel/types.h>
#include <pmos/load_data.h>
#include <pmos/tls.h>
#include <string.h>
#include "auxvec.h"
#include <elf.h>
#include "loader.h"

static const uint64_t page_mask = PAGE_SIZE - 1;

result_t load_executable(uint64_t task_id, uint64_t mem_object_id, unsigned flags,
                         void *userspace_tags, size_t userspace_tags_size,
                         const char *argv[], const char *envp[], const struct AuxVecEntry *auxvec_entries[])
{
    struct AuxVecBuilder *builder = NULL;
    auto size_r = get_mem_object_size(mem_object_id, 0);
    if (size_r.result)
        return size_r.result;
    uint64_t mem_object_size = size_r.value;

    if (mem_object_size == 0)
        return -EFAULT;

    uint8_t *auxvec_data = NULL;

    result_t result = 0;
    void *file_mapped = NULL;

    auto mem_request = map_mem_object(&(map_mem_object_param_t){
        .page_table_id = 0,
        .object_id = mem_object_id,
        .addr_start_uint = 0,
        .size = mem_object_size,
        .offset_object = 0,
        .offset_start = 0,
        .object_size = mem_object_size,
        .access_flags = PROT_READ,
    });

    if (mem_request.result)
        return mem_request.result;
    file_mapped = mem_request.virt_addr;

    ELF_Common *header = file_mapped;
    if (header->magic != ELF_MAGIC) {
        result = -ENOEXEC;
        goto error;
    }

    if (header->endianness != ELF_ENDIANNESS) {
        result = -ENOEXEC;
        goto error;
    }

    if (header->type != ELF_EXEC) {
        result = -ENOEXEC;
        goto error;
    }

    builder = auxvec_new();
    if (!builder) {
        result = -ENOMEM;
        goto error;
    }

    page_table_req_ret_t pt_request = assign_page_table(task_id, 0, PAGE_TABLE_CREATE, header->instr_set);
    if (pt_request.result) {
        result = pt_request.result;
        goto error;
    }
    pmos_pagetable_t page_table_id = pt_request.page_table;

    uint64_t tls_memsz = 0;
    uint64_t tls_align = 0;
    uint64_t tls_filesz = 0;
    uint64_t tls_offset = 0;

    uint64_t program_entry = 0;

    struct load_tag_elf_phdr phdr_tag = {LOAD_TAG_ELF_PHDR_HEADER, 0, 0, 0, 0};

    if (header->bitness == ELF_32BIT) {
        ELF_32bit *header = file_mapped;

        uint32_t pheader_count = header->program_header_entries;
        uint32_t pheader_size = pheader_count * sizeof(ELF_32bit);
        uint32_t offset = header->program_header;

        program_entry = header->program_entry;

        if (offset + pheader_size > mem_object_size) {
            result = -EFAULT;
            goto error;
        }

        phdr_tag.phdr_num     = pheader_count;
        phdr_tag.phdr_size    = sizeof(ELF_PHeader_32);

        const ELF_PHeader_32 *pheader = (ELF_PHeader_32 *)((char *)file_mapped + offset);
        for (uint32_t i = 0; i < pheader_count; ++i) {
            const ELF_PHeader_32 *ph = pheader + i;

            if (ph->type == PT_TLS) {
                tls_memsz = ph->p_memsz;
                tls_align = ph->alignment;
                tls_filesz = ph->p_filesz;
                tls_offset = ph->p_offset;
            }

            if (ph->type != ELF_SEGMENT_LOAD)
                continue;

            if ((ph->p_vaddr & 0xfff) != (ph->p_offset & 0xfff)) {
                result = -ENOEXEC;
                goto error;
            }

            // If pheader is inside this segment, set its virtual address
            if (ph->p_offset <= header->program_header &&
                header->program_header + pheader_size <= ph->p_offset + ph->p_filesz) {
                phdr_tag.phdr_addr = ph->p_vaddr + header->program_header - ph->p_offset;
            }

            if (!(ph->flags & ELF_FLAG_WRITABLE)) {
                // Direct map the region
                const uint32_t region_start = ph->p_vaddr & ~page_mask;
                const uint32_t file_offset  = ph->p_offset & ~page_mask;
                const uint32_t size         = ((ph->p_vaddr & page_mask) + ph->p_memsz + page_mask) & ~page_mask;
            
                unsigned protection = CREATE_FLAG_FIXED;
                if (ph->flags & ELF_FLAG_EXECUTABLE)
                    protection |= PROT_EXEC;
                if (ph->flags & ELF_FLAG_READABLE)
                    protection |= PROT_READ;

                auto mem_request = map_mem_object(&(map_mem_object_param_t){
                    .page_table_id = page_table_id,
                    .object_id = mem_object_id,
                    .addr_start_uint = region_start,
                    .size = size,
                    .offset_object = file_offset,
                    .offset_start = 0,
                    .object_size = size,
                    .access_flags = protection,
                });
                if (mem_request.result) {
                    result = mem_request.result;
                    goto error;
                }
            } else {
                // Copy the region on access
                const uint32_t region_start = ph->p_vaddr & ~page_mask;
                const uint32_t size         = ((ph->p_vaddr & page_mask) + ph->p_memsz + page_mask) & ~page_mask;
                const uint32_t file_offset         = ph->p_offset;
                const uint32_t file_size           = ph->p_filesz;
                const uint32_t object_start_offset = ph->p_vaddr - region_start;

                unsigned protection = CREATE_FLAG_COW | CREATE_FLAG_FIXED | PROT_WRITE;
                if (ph->flags & ELF_FLAG_EXECUTABLE)
                    protection |= PROT_EXEC;
                if (ph->flags & ELF_FLAG_READABLE)
                    protection |= PROT_READ;

                auto mem_request = map_mem_object(&(map_mem_object_param_t){
                    .page_table_id = page_table_id,
                    .object_id = mem_object_id,
                    .addr_start_uint = region_start,
                    .size = size,
                    .offset_object = file_offset,
                    .offset_start = object_start_offset,
                    .object_size = file_size,
                    .access_flags = protection,
                });
                if (mem_request.result) {
                    result = mem_request.result;
                    goto error;
                }
            }
        }
    } else {
        builder->ptr_is_64bit = true;
        ELF_64bit *header = file_mapped;

        uint64_t pheader_count = header->program_header_entries;
        uint64_t pheader_size = pheader_count * sizeof(*header);
        uint64_t offset = header->program_header;

        program_entry = header->program_entry;

        if (offset + pheader_size > mem_object_size) {
            result = -EFAULT;
            goto error;
        }

        phdr_tag.phdr_num     = pheader_count;
        phdr_tag.phdr_size    = sizeof(ELF_PHeader_64);

        const ELF_PHeader_64 *pheader = (ELF_PHeader_64 *)((char *)file_mapped + offset);
        for (uint64_t i = 0; i < pheader_count; ++i) {
            const ELF_PHeader_64 *ph = pheader + i;

            if (ph->type == PT_TLS) {
                tls_memsz = ph->p_memsz;
                tls_align = ph->alignment;
                tls_filesz = ph->p_filesz;
                tls_offset = ph->p_offset;
            }

            if (ph->type != ELF_SEGMENT_LOAD)
                continue;

            if ((ph->p_vaddr & 0xfff) != (ph->p_offset & 0xfff)) {
                result = -ENOEXEC;
                goto error;
            }

            // If pheader is inside this segment, set its virtual address
            if (ph->p_offset <= header->program_header &&
                header->program_header + pheader_size <= ph->p_offset + ph->p_filesz) {
                phdr_tag.phdr_addr = ph->p_vaddr + header->program_header - ph->p_offset;
            }

            if (!(ph->flags & ELF_FLAG_WRITABLE)) {
                // Direct map the region
                const uint64_t region_start = ph->p_vaddr & ~page_mask;
                const uint64_t file_offset  = ph->p_offset & ~page_mask;
                const uint64_t size         = ((ph->p_vaddr & page_mask) + ph->p_memsz + page_mask) & ~page_mask;
            
                unsigned protection = CREATE_FLAG_FIXED;
                if (ph->flags & ELF_FLAG_EXECUTABLE)
                    protection |= PROT_EXEC;
                if (ph->flags & ELF_FLAG_READABLE)
                    protection |= PROT_READ;

                auto mem_request = map_mem_object(&(map_mem_object_param_t){
                    .page_table_id = page_table_id,
                    .object_id = mem_object_id,
                    .addr_start_uint = region_start,
                    .size = size,
                    .offset_object = file_offset,
                    .offset_start = 0,
                    .object_size = size,
                    .access_flags = protection,
                });
                if (mem_request.result) {
                    result = mem_request.result;
                    goto error;
                }
            } else {
                // Copy the region on access
                const uint64_t region_start = ph->p_vaddr & ~page_mask;
                const uint64_t size         = ((ph->p_vaddr & page_mask) + ph->p_memsz + page_mask) & ~page_mask;
                const uint64_t file_offset         = ph->p_offset;
                const uint64_t file_size           = ph->p_filesz;
                const uint64_t object_start_offset = ph->p_vaddr - region_start;

                unsigned protection = CREATE_FLAG_COW | CREATE_FLAG_FIXED | PROT_WRITE;
                if (ph->flags & ELF_FLAG_EXECUTABLE)
                    protection |= PROT_EXEC;
                if (ph->flags & ELF_FLAG_READABLE)
                    protection |= PROT_READ;

                auto mem_request = map_mem_object(&(map_mem_object_param_t){
                    .page_table_id = page_table_id,
                    .object_id = mem_object_id,
                    .addr_start_uint = region_start,
                    .size = size,
                    .offset_object = file_offset,
                    .offset_start = object_start_offset,
                    .object_size = file_size,
                    .access_flags = protection,
                });
                if (mem_request.result) {
                    result = mem_request.result;
                    goto error;
                }
            }
        }
    }

    uint64_t tls_addr = 0;

    // TLS Stuff (TODO: Remove it)
    if (tls_memsz > 0) {
        if (tls_offset + tls_filesz > mem_object_size) {
            result = -EFAULT;
            goto error;
        }

        const size_t size = sizeof(TLS_Data) + ((tls_memsz + 7) & ~7UL);
        
        uint64_t tls_pa_size = (size + page_mask) & ~page_mask;
        auto tls_result = create_normal_region(TASK_ID_SELF, NULL, tls_pa_size, PROT_READ | PROT_WRITE);
        if (tls_result.result) {
            result = tls_result.result;
            goto error;
        }

        TLS_Data *tls_data = tls_result.virt_addr;

        tls_data->memsz  = tls_memsz;
        tls_data->align  = tls_align;
        tls_data->filesz = tls_align;

        memcpy(tls_data->data, (char *)file_mapped + tls_offset, tls_filesz);

        auto res = transfer_region(page_table_id, tls_result.virt_addr, 0, PROT_READ);
        if (res.result) {
            release_region(TASK_ID_SELF, tls_result.virt_addr);
            result = res.result;
            goto error;
        }
        tls_addr = res.virt_addr_intptr;
    }


    size_t stack_size = MB(16);
    // Init stack
    auto stack_result = create_normal_region(task_id, nullptr, stack_size, PROT_NONE);
    if (stack_result.result) {
        result = stack_result.result;
        goto error;
    }

    // Stack
    int push_res;
    VECTOR_PUSH_BACK_CHECKED(builder->entries, ((struct AuxVecEntry){
        .entry_type = AT_USRSTACKLIM,
        .data_type = DATA_TYPE_LONG,
        .long_data = stack_size,
    }), push_res);
    if (push_res) {
        result = push_res;
        goto error;
    }

    VECTOR_PUSH_BACK_CHECKED(builder->entries, ((struct AuxVecEntry){
        .entry_type = AT_USRSTACKBASE,
        .data_type = DATA_TYPE_PTR,
        .ptr = stack_result.virt_addr_intptr + stack_size,
    }), push_res);
    if (push_res) {
        result = push_res;
        goto error;
    }

    // PHDR
        VECTOR_PUSH_BACK_CHECKED(builder->entries, ((struct AuxVecEntry){
        .entry_type = AT_PHDR,
        .data_type = DATA_TYPE_PTR,
        .ptr = phdr_tag.phdr_addr,
    }), push_res);
    if (push_res) {
        result = push_res;
        goto error;
    }
    VECTOR_PUSH_BACK_CHECKED(builder->entries, ((struct AuxVecEntry){
        .entry_type = AT_PHENT,
        .data_type = DATA_TYPE_LONG,
        .long_data = phdr_tag.phdr_size,
    }), push_res);
    if (push_res) {
        result = push_res;
        goto error;
    }
    VECTOR_PUSH_BACK_CHECKED(builder->entries, ((struct AuxVecEntry){
        .entry_type = AT_PHNUM,
        .data_type = DATA_TYPE_LONG,
        .long_data = phdr_tag.phdr_num,
    }), push_res);
    if (push_res) {
        result = push_res;
        goto error;
    }


    struct load_tag_stack_descriptor sd = {
        .header = LOAD_TAG_STACK_DESCRIPTOR_HEADER,
        .stack_top = stack_result.virt_addr_intptr + stack_size,
        .stack_size = stack_size,
        .guard_size = 0,
        .unused0 = 0,
    };

    struct load_tag_userspace_tags ut = {
        .header = LOAD_TAG_USERSPACE_TAGS_HEADER,
        .tags_address = 0,
        .memory_size = 0,
    };

    struct load_tag_mem_object_id mo = {
        .header = LOAD_TAG_MEM_OBJECT_ID_HEADER,
        .memory_object_id = mem_object_id,
    };

    VECTOR_PUSH_BACK_CHECKED(builder->entries, ((struct AuxVecEntry){
        .entry_type = AT_MEM_OBJ_ID,
        .data_type = DATA_TYPE_EXTERNAL,
        .external_data = {
            .size = sizeof(mem_object_id),
            .data = &mem_object_id,
        },
    }), push_res);
    if (push_res) {
        result = push_res;
        goto error;
    }

    push_res = auxvec_push_argv(builder, argv);
    if (push_res) {
        result = push_res;
        goto error;
    }
    push_res = auxvec_push_envp(builder, envp);
    if (push_res) {
        result = push_res;
        goto error;
    }

    if (auxvec_entries) {
        auto ptr = auxvec_entries;
        while (*ptr) {
            VECTOR_PUSH_BACK_CHECKED(builder->entries, **ptr, push_res);
            if (push_res) {
                result = push_res;
                goto error;
            }
            ++ptr;
        }
    }

    size_t load_tags_size = sizeof(struct load_tag_elf_phdr) + sizeof(struct load_tag_close) + sizeof(struct load_tag_stack_descriptor) + sizeof(struct load_tag_mem_object_id);

    if (userspace_tags) {
        load_tags_size += sizeof(struct load_tag_userspace_tags);

        auto res = transfer_region(page_table_id, userspace_tags, 0, PROT_READ | PROT_WRITE);
        if (res.result) {
            release_region(task_id, userspace_tags);
            result = res.result;
            goto error;
        }
        ut.tags_address = res.virt_addr_intptr;
        ut.memory_size = userspace_tags_size;
    }

    size_t mtsall = (load_tags_size + page_mask) & ~page_mask;
    auto tags_result = create_normal_region(TASK_ID_SELF, NULL, mtsall, PROT_READ | PROT_WRITE);
    if (tags_result.result) {
        result = tags_result.result;
        goto error;
    }

    memcpy(tags_result.virt_addr, &phdr_tag, sizeof(phdr_tag));
    memcpy((char *)tags_result.virt_addr + sizeof(phdr_tag), &sd, sizeof(struct load_tag_stack_descriptor));
    memcpy((char *)tags_result.virt_addr + sizeof(phdr_tag) + sizeof(sd), &mo, sizeof(mo));


    if (userspace_tags) {
        memcpy((char *)tags_result.virt_addr + sizeof(phdr_tag) + sizeof(struct load_tag_stack_descriptor) + sizeof(mo), &ut, sizeof(ut));
    }

    struct load_tag_close ltgcl = {
        .header = LOAD_TAG_CLOSE_HEADER,
    };

    memcpy((char *)tags_result.virt_addr + load_tags_size - sizeof(ltgcl), &ltgcl, sizeof(ltgcl));

    auto res = transfer_region(page_table_id, tags_result.virt_addr, 0, PROT_READ | PROT_WRITE);
    if (res.result) {
        release_region(task_id, tags_result.virt_addr);
        result = res.result;
        goto error;
    }

    // // Stack stuff
    size_t auxvec_size = 0;
    int serial_result = auxvec_serialize(builder, stack_result.virt_addr_intptr, &auxvec_data, &auxvec_size);
    if (serial_result) {
        result = serial_result;
        goto error;
    }
    auto s_res = create_normal_region(TASK_ID_SELF, NULL, stack_size, PROT_READ | PROT_WRITE);
    if (s_res.result) {
        result = s_res.result;
        goto error;
    }

    memcpy((char *)s_res.virt_addr + stack_size - auxvec_size, auxvec_data, auxvec_size);
    s_res = transfer_region(page_table_id, s_res.virt_addr, stack_result.virt_addr_intptr, CREATE_FLAG_FIXED | PROT_READ | PROT_WRITE);
    if (s_res.result) {
        release_region(TASK_ID_SELF, s_res.virt_addr);
        result = s_res.result;
        goto error;
    }

    auto sr = init_stack(task_id, stack_result.virt_addr_intptr + stack_size - auxvec_size);
    if (sr.result) {
        result = sr.result;
        goto error;
    }


    auto start_result = syscall_start_process(task_id, program_entry, res.virt_addr_intptr, load_tags_size, tls_addr);
    if (start_result) {
        result = start_result;
        goto error;
    }

error:
    free(auxvec_data);
    auxvec_free(builder);
    if (file_mapped)
        release_region(0, file_mapped);

    return result;
}