#include <pmos/system.h>
#include <pmos/memory.h>
#include <elf.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <kernel/types.h>
#include <pmos/load_data.h>
#include <string.h>
#include "auxvec.h"
#include <elf.h>
#include "loader.h"
#include <sys/user.h>
#include <sys/mman.h>
#include <pmos/ports.h>
#include <pmos/fs-data.h>
#include "../io.h"

extern pmos_right_t posix_server_right;

static const uint64_t page_mask = PAGE_SIZE - 1;

#define ELF_ENDIANNESS 1
#ifdef __x86_64__
#define ELF_INSTR_SET EM_X86_64
#elif defined(__i386__)
#define ELF_INSTR_SET EM_386
#elif defined(__riscv)
#define ELF_INSTR_SET EM_RISCV
#elif defined(__loongarch__)
#define ELF_INSTR_SET EM_LOONGARCH
#endif

result_t add_posix_stuff(struct AuxVecBuilder *builder, uint64_t task_group_id)
{
    if (posix_server_right == INVALID_RIGHT)
        // Just don't pass it
        return 0;

    auto result = dup_right(posix_server_right);
    if (result.result)
        return result.result;

    auto transfer_result = transfer_right(task_group_id, result.right, 0);
    if (transfer_result.result) {
        delete_right(result.right);
        return transfer_result.result;
    }

    // This server is single threaded so this is fine
    static uint64_t posix_right_id = 0;

    int push_res = 0;
    VECTOR_PUSH_BACK_CHECKED(builder->entries, ((struct AuxVecEntry){
        .entry_type = AT_POSIX_RIGHT,
        .data_type = DATA_TYPE_EXTERNAL,
        .external_data = {
            .size = sizeof(posix_right_id),
            .data = &posix_right_id,
        },
    }), push_res);
    if (push_res) {
        return -ENOMEM;
    }

    return 0;
}

extern pmos_right_t stdout_pipe[2], stderr_pipe[2];

result_t clone_right_to(uint64_t task_group_id, pmos_right_t *right, uint64_t *out_right_id)
{
    auto dup_result = dup_right(*right);
    if (dup_result.result) {
        // If the right gets deleted, don't fail the process creation, and just don't pass this to it
        if (dup_result.result == (result_t)-ENOENT) {
            *right = INVALID_RIGHT;
            return 0;
        }
    
        return dup_result.result;
    }

    auto transfer_result = transfer_right(task_group_id, dup_result.right, 0);
    if (transfer_result.result) {
        if (transfer_result.result == (result_t)-ENOENT) {
            // Same as above
            *right = INVALID_RIGHT;
            return 0;
        }

        delete_right(dup_result.right);
        return transfer_result.result;
    }

    *out_right_id = transfer_result.right;
    return 0;
}

result_t pass_filesystem(struct AuxVecBuilder *builder, uint64_t page_table_id, uint64_t task_group_id)
{
    uint64_t size = PAGE_SIZE;
    void *page = NULL;
    auto s_res = create_normal_region(TASK_ID_SELF, NULL, size, PROT_READ | PROT_WRITE);
    if (s_res.result)
        return s_res.result;

    page = s_res.virt_addr;

    auto fs_data = (struct PmosFsData *)page;
    fs_data->total_size = size;
    fs_data->array_size = 3;

    result_t result;
    if (stdout_pipe[1] && (result = clone_right_to(task_group_id, &stdout_pipe[1], &fs_data->open_files[1].io_right)))
        goto error;
    if (stdout_pipe[1] && (result = clone_right_to(task_group_id, &stdout_pipe[1], &fs_data->open_files[1].op_right)))
        goto error;

    // Set ISATTY even though it is a pipe
    fs_data->open_files[1].flags |= FLAG_ISATTY;
    fs_data->open_files[1].flags |= FLAG_ISPIPE;
    
    if (stderr_pipe[1] && (result = clone_right_to(task_group_id, &stderr_pipe[1], &fs_data->open_files[2].io_right)))
        goto error;
    if (stderr_pipe[1] && (result = clone_right_to(task_group_id, &stderr_pipe[1], &fs_data->open_files[2].op_right)))
        goto error;

    fs_data->open_files[2].flags |= FLAG_ISATTY;
    fs_data->open_files[2].flags |= FLAG_ISPIPE;
    
    auto move_result = transfer_region(page_table_id, page, 0, PROT_READ | PROT_WRITE);
    if (move_result.result) {
        result = move_result.result;
        goto error;
    }
    page = NULL;

    int push_res = 0;
    VECTOR_PUSH_BACK_CHECKED(builder->entries, ((struct AuxVecEntry){
        .entry_type = AT_FD_TABLE,
        .data_type = DATA_TYPE_PTR,
        .ptr = move_result.virt_addr_intptr,
    }), push_res);
    if (push_res) {
        result = push_res;
        goto error;
    }

    return 0;

error:
    if (page)
        release_memory_range(TASK_ID_SELF, page, size);

    return result;
}

uint64_t get_program_entry(void *file_mapped)
{
    Elf32_Ehdr *header = file_mapped;
    if (header->e_ident[4] == R_LARCH_32) {
        Elf32_Ehdr *header = file_mapped;
        return header->e_entry;
    } else {
        Elf64_Ehdr *header = file_mapped;
        return header->e_entry;
    }
}

uint64_t get_phdr_addr(void *file_mapped)
{
    Elf32_Ehdr *header = file_mapped;
    if (header->e_ident[4] == R_LARCH_32) {
        Elf32_Ehdr *header = file_mapped;
        uint32_t pheader_size = header->e_phentsize;
        for (uint32_t i = 0; i < header->e_phnum; ++i) {
            const Elf32_Phdr *ph = (const Elf32_Phdr *)((char *)file_mapped + header->e_phoff + i * pheader_size);
            if (ph->p_type == PT_PHDR)
                return ph->p_vaddr;
        }
    } else {
        Elf64_Ehdr *header = file_mapped;
        uint64_t pheader_size = header->e_phentsize;
        for (uint64_t i = 0; i < header->e_phnum; ++i) {
            const Elf64_Phdr *ph = (const Elf64_Phdr *)((char *)file_mapped + header->e_phoff + i * pheader_size);
            if (ph->p_type == PT_PHDR)
                return ph->p_vaddr;
        }
    }
    return 0;
}

bool is_relocatable(void *file_mapped)
{
    Elf32_Ehdr *header = file_mapped;
    return header->e_type == ET_DYN;
}

const char *find_interpreter(void *file_mapped)
{
    Elf32_Ehdr *header = file_mapped;
    if (header->e_ident[4] == R_LARCH_32) {
        Elf32_Ehdr *header = file_mapped;
        uint32_t pheader_size = header->e_phentsize;
        for (uint32_t i = 0; i < header->e_phnum; ++i) {
            const Elf32_Phdr *ph = (const Elf32_Phdr *)((char *)file_mapped + header->e_phoff + i * pheader_size);
            if (ph->p_type == PT_INTERP)
                // TODO: This is an easy buffer overflow on malformed ELFs
                return (const char *)file_mapped + ph->p_offset;
        
            if (ph->p_type == PT_LOAD)
                // Interpreter segments must precede loadable segments per spec
                break;
        }
    } else {
        Elf64_Ehdr *header = file_mapped;
        uint64_t pheader_size = header->e_phentsize;
        for (uint64_t i = 0; i < header->e_phnum; ++i) {
            const Elf64_Phdr *ph = (const Elf64_Phdr *)((char *)file_mapped + header->e_phoff + i * pheader_size);
            if (ph->p_type == PT_INTERP)
                return (const char *)file_mapped + ph->p_offset;
        
            if (ph->p_type == PT_LOAD)
                break;
        }
    }
    return NULL;
}
        
void find_min_max_page(void *file_mapped, uint64_t *min_page, uint64_t *max_page)
{
    Elf32_Ehdr *header = file_mapped;
    *min_page = UINT64_MAX;
    *max_page = 0;
    if (header->e_ident[4] == R_LARCH_32) {
        Elf32_Ehdr *header = file_mapped;
        uint32_t pheader_size = header->e_phentsize;
        for (uint32_t i = 0; i < header->e_phnum; ++i) {
            const Elf32_Phdr *ph = (const Elf32_Phdr *)((char *)file_mapped + header->e_phoff + i * pheader_size);
            if (ph->p_type != PT_LOAD)
                continue;

            uint64_t start_page = ph->p_vaddr & ~page_mask;
            uint64_t end_page   = ((ph->p_vaddr & page_mask) + ph->p_memsz + page_mask) & ~page_mask;

            if (start_page < *min_page)
                *min_page = start_page;
            if (end_page > *max_page)
                *max_page = end_page;
        }
    } else {
        Elf64_Ehdr *header = file_mapped;
        uint64_t pheader_size = header->e_phentsize;
        for (uint64_t i = 0; i < header->e_phnum; ++i) {
            const Elf64_Phdr *ph = (const Elf64_Phdr *)((char *)file_mapped + header->e_phoff + i * pheader_size);
            if (ph->p_type != PT_LOAD)
                continue;

            uint64_t start_page = ph->p_vaddr & ~page_mask;
            uint64_t end_page   = ((ph->p_vaddr & page_mask) + ph->p_memsz + page_mask) & ~page_mask;

            if (start_page < *min_page)
                *min_page = start_page;
            if (end_page > *max_page)
                *max_page = end_page;
        }
    }
}

mem_object_t find_file(const char *path);

int check_elf_file(void *file_mapped, int *e_machine)
{
    Elf32_Ehdr *header = file_mapped;
    if (memcmp(header->e_ident, ELFMAG, SELFMAG)) {
        return -ENOEXEC;
    }

    if (header->e_ident[5] != ELF_ENDIANNESS) {
        return -ENOEXEC;
    }

    if (header->e_type != ET_EXEC && header->e_type != ET_DYN) {
        return -ENOEXEC;
    }

    if (e_machine && header->e_machine != *e_machine) {
        print_str("ELF file has wrong machine type: ");
        print_hex(header->e_machine);
        print_str(", expected: ");
        print_hex(*e_machine);
        print_str("\n");
        return -ENOEXEC;
    }

    return 0;
}

result_t load_elf_to_memory(void *file_mapped, uint64_t mem_object_id, uint64_t mem_object_size, uint64_t page_table_id, uint64_t *relocation_offset)
{
    result_t result = 0;

    Elf32_Ehdr *header = file_mapped;
    if (memcmp(header->e_ident, ELFMAG, SELFMAG)) {
        result = -ENOEXEC;
        goto error;
    }

    if (header->e_ident[5] != ELF_ENDIANNESS) {
        result = -ENOEXEC;
        goto error;
    }

    if (header->e_type != ET_EXEC && header->e_type != ET_DYN) {
        result = -ENOEXEC;
        goto error;
    }

    if (is_relocatable(file_mapped)) {
        // Try relocating
        uint64_t min_page, max_page;
        find_min_max_page(file_mapped, &min_page, &max_page);
        auto c_result = create_normal_region64(page_table_id, min_page, max_page - min_page, PROT_READ | PROT_WRITE | PROT_EXEC);
        if (c_result.result) {
            result = c_result.result;
            goto error;
        }

        *relocation_offset = c_result.virt_addr_intptr - min_page;

        release_memory_range64(page_table_id, min_page, max_page - min_page);
    } else {
        *relocation_offset = 0;
    }

    if (header->e_ident[4] == R_LARCH_32) {
        Elf32_Ehdr *header = file_mapped;

        uint32_t pheader_count = header->e_phnum;
        uint32_t pheader_size = pheader_count * sizeof(Elf32_Phdr);
        uint32_t offset = header->e_phoff;

        if (offset + pheader_size > mem_object_size) {
            result = -EFAULT;
            goto error;
        }

        const Elf32_Phdr *pheader = (Elf32_Phdr *)((char *)file_mapped + offset);
        for (uint32_t i = 0; i < pheader_count; ++i) {
            const Elf32_Phdr *ph = pheader + i;

            if (ph->p_type != PT_LOAD)
                continue;

            if ((ph->p_vaddr & 0xfff) != (ph->p_offset & 0xfff)) {
                result = -ENOEXEC;
                goto error;
            }

            if (!(ph->p_flags & PF_W)) {
                // Direct map the region
                const uint32_t region_start = ph->p_vaddr & ~page_mask;
                const uint32_t file_offset  = ph->p_offset & ~page_mask;
                const uint32_t size         = ((ph->p_vaddr & page_mask) + ph->p_memsz + page_mask) & ~page_mask;
            
                unsigned protection = CREATE_FLAG_FIXED;
                if (ph->p_flags & PF_X)
                    protection |= PROT_EXEC;
                if (ph->p_flags & PF_R)
                    protection |= PROT_READ;

                auto mem_request = map_mem_object(&(map_mem_object_param_t){
                    .page_table_id = page_table_id,
                    .object_right = mem_object_id,
                    .addr_start_uint = region_start + *relocation_offset,
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
                if (ph->p_flags & PF_X)
                    protection |= PROT_EXEC;
                if (ph->p_flags & PF_R)
                    protection |= PROT_READ;

                auto mem_request = map_mem_object(&(map_mem_object_param_t){
                    .page_table_id = page_table_id,
                    .object_right = mem_object_id,
                    .addr_start_uint = region_start + *relocation_offset,
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
        Elf64_Ehdr *header = file_mapped;

        uint64_t pheader_count = header->e_phnum;
        uint64_t pheader_size = pheader_count * sizeof(*header);
        uint64_t offset = header->e_phoff;

        if (offset + pheader_size > mem_object_size) {
            result = -EFAULT;
            goto error;
        }

        const Elf64_Phdr *pheader = (Elf64_Phdr *)((char *)file_mapped + offset);
        for (uint64_t i = 0; i < pheader_count; ++i) {
            const Elf64_Phdr *ph = pheader + i;

            if (ph->p_type != PT_LOAD)
                continue;

            if ((ph->p_vaddr & 0xfff) != (ph->p_offset & 0xfff)) {
                result = -ENOEXEC;
                goto error;
            }

            if (!(ph->p_flags & PF_W)) {
                // Direct map the region
                const uint64_t region_start = ph->p_vaddr & ~page_mask;
                const uint64_t file_offset  = ph->p_offset & ~page_mask;
                const uint64_t size         = ((ph->p_vaddr & page_mask) + ph->p_memsz + page_mask) & ~page_mask;
            
                unsigned protection = CREATE_FLAG_FIXED;
                if (ph->p_flags & PF_X)
                    protection |= PROT_EXEC;
                if (ph->p_flags & PF_R)
                    protection |= PROT_READ;

                auto mem_request = map_mem_object(&(map_mem_object_param_t){
                    .page_table_id = page_table_id,
                    .object_right = mem_object_id,
                    .addr_start_uint = region_start + *relocation_offset,
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
                if (ph->p_flags & PF_X)
                    protection |= PROT_EXEC;
                if (ph->p_flags & PF_R)
                    protection |= PROT_READ;

                auto mem_request = map_mem_object(&(map_mem_object_param_t){
                    .page_table_id = page_table_id,
                    .object_right = mem_object_id,
                    .addr_start_uint = region_start + *relocation_offset,
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
error:
    return result;
}

result_t load_executable(uint64_t task_id, uint64_t group_id, uint64_t mem_object_id, unsigned flags,
                         void *userspace_tags, size_t userspace_tags_size,
                         const char *argv[], const char *envp[], const struct AuxVecEntry *auxvec_entries[])
{
    struct AuxVecBuilder *builder = NULL;
    auto size_r = get_mem_object_size(mem_object_id, 0);
    if (size_r.result)
        return size_r.result;
    uint64_t mem_object_size = size_r.value;
    uint64_t interp_object_size = 0;

    uint64_t program_rel_offset = 0, interp_rel_offset = 0;

    if (mem_object_size == 0)
        return -EFAULT;

    uint8_t *auxvec_data = NULL;

    result_t result = 0;
    void *file_mapped = NULL;
    void *interp_mapped = NULL;

    auto mem_request = map_mem_object(&(map_mem_object_param_t){
        .page_table_id = 0,
        .object_right = mem_object_id,
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

    Elf32_Ehdr *header = file_mapped;
    if (memcmp(header->e_ident, ELFMAG, SELFMAG)) {
        result = -ENOEXEC;
        goto error;
    }

    if (header->e_ident[5] != ELF_ENDIANNESS) {
        result = -ENOEXEC;
        goto error;
    }

    if (header->e_type != ET_EXEC && header->e_type != ET_DYN) {
        result = -ENOEXEC;
        goto error;
    }

    builder = auxvec_new();
    if (!builder) {
        result = -ENOMEM;
        goto error;
    }

    page_table_req_ret_t pt_request = assign_page_table(task_id, 0, PAGE_TABLE_CREATE, header->e_machine);
    if (pt_request.result) {
        result = pt_request.result;
        goto error;
    }
    pmos_pagetable_t page_table_id = pt_request.page_table;

    uint64_t program_entry = 0, interp_entry = 0;

    unsigned pheader_count = 0;
    unsigned pheader_size = 0;
    uint64_t pheader_offset = 0;

    if (header->e_ident[4] == R_LARCH_32) {
        Elf32_Ehdr *header = file_mapped;

        pheader_count = header->e_phnum;
        pheader_size = header->e_phentsize;

        if (pheader_offset + pheader_size * pheader_count > mem_object_size) {
            result = -EFAULT;
            goto error;
        }
    } else {
        builder->ptr_is_64bit = true;
        Elf64_Ehdr *header = file_mapped;

        pheader_count = header->e_phnum;
        pheader_size = header->e_phentsize;

        if (pheader_offset + pheader_size * pheader_count > mem_object_size) {
            result = -EFAULT;
            goto error;
        }
    }

    program_entry = get_program_entry(file_mapped);
    // TODO!
    pheader_offset = get_phdr_addr(file_mapped);

    uint64_t at_base = 0;

    bool load_interp_first = false;
    auto interp = find_interpreter(file_mapped);
    uint64_t interp_object = 0;
    if (interp) {
        print_str("Found interpreter: ");
        print_str(interp);
        print_str("\n");

        interp_object = find_file(interp);
        if (!interp_object) {
            print_str("Failed to find interpreter file\n");
            result = -ENOENT;
            goto error;
        }

        auto interp_size_r = get_mem_object_size(interp_object, 0);
        if (interp_size_r.result) {
            result = interp_size_r.result;
            goto error;
        }
        interp_object_size = interp_size_r.value;

        auto interp_map_r = map_mem_object(&(map_mem_object_param_t){
            .page_table_id = 0,
            .object_right = interp_object,
            .addr_start_uint = 0,
            .size = interp_object_size,
            .offset_object = 0,
            .offset_start = 0,
            .object_size = interp_object_size,
            .access_flags = PROT_READ,
        });
        if (interp_map_r.result) {
            result = interp_map_r.result;
            goto error;
        }
        interp_mapped = interp_map_r.virt_addr;

        int e_machine = header->e_machine;
        if ((result = check_elf_file(interp_map_r.virt_addr, &e_machine))) {
            goto error;
        }

        load_interp_first = !is_relocatable(interp_map_r.virt_addr) && is_relocatable(file_mapped);
        interp_entry = get_program_entry(interp_map_r.virt_addr);

        uint64_t base_addr = 0, end_addr = 0;
        find_min_max_page(interp_map_r.virt_addr, &base_addr, &end_addr);
        at_base = base_addr;
    }

    if (interp_mapped && load_interp_first) {
        if ((result = load_elf_to_memory(interp_mapped, interp_object, interp_object_size, page_table_id, &interp_rel_offset)))
            goto error;
    }

    if ((result = load_elf_to_memory(file_mapped, mem_object_id, mem_object_size, page_table_id, &program_rel_offset)))
        goto error;

    if (interp_mapped && !load_interp_first) {
        if ((result = load_elf_to_memory(interp_mapped, interp_object, interp_object_size, page_table_id, &interp_rel_offset)))
            goto error;
    }

    if (program_rel_offset) {
        print_str("Relocated program by +");
        print_hex(program_rel_offset);
        print_str("\n");
    }
    if (interp_rel_offset) {
        print_str("Relocated interpreter by +");
        print_hex(interp_rel_offset);
        print_str("\n");
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
        .ptr = pheader_offset + program_rel_offset,
    }), push_res);
    if (push_res) {
        result = push_res;
        goto error;
    }
    VECTOR_PUSH_BACK_CHECKED(builder->entries, ((struct AuxVecEntry){
        .entry_type = AT_PHENT,
        .data_type = DATA_TYPE_LONG,
        .long_data = pheader_size,
    }), push_res);
    if (push_res) {
        result = push_res;
        goto error;
    }
    VECTOR_PUSH_BACK_CHECKED(builder->entries, ((struct AuxVecEntry){
        .entry_type = AT_PHNUM,
        .data_type = DATA_TYPE_LONG,
        .long_data = pheader_count,
    }), push_res);
    if (push_res) {
        result = push_res;
        goto error;
    }
    VECTOR_PUSH_BACK_CHECKED(builder->entries, ((struct AuxVecEntry){
        .entry_type = AT_PAGESZ,
        .data_type = DATA_TYPE_LONG,
        .long_data = 4096, // TODO...
    }), push_res);
    if (push_res) {
        result = push_res;
        goto error;
    }
    VECTOR_PUSH_BACK_CHECKED(builder->entries, ((struct AuxVecEntry){
        .entry_type = AT_ENTRY,
        .data_type = DATA_TYPE_PTR,
        .ptr = program_entry + program_rel_offset,
    }), push_res);
    if (push_res) {
        result = push_res;
        goto error;
    }

    if (interp_mapped) {
        VECTOR_PUSH_BACK_CHECKED(builder->entries, ((struct AuxVecEntry){
            .entry_type = AT_BASE,
            .data_type = DATA_TYPE_PTR,
            .ptr = at_base + interp_rel_offset,
        }), push_res);
        if (push_res) {
            result = push_res;
            goto error;
        }
    }

    // VECTOR_PUSH_BACK_CHECKED(builder->entries, ((struct AuxVecEntry){
    //     .entry_type = AT_MEM_OBJ_ID,
    //     .data_type = DATA_TYPE_EXTERNAL,
    //     .external_data = {
    //         .size = sizeof(mem_object_id),
    //         .data = &mem_object_id,
    //     },
    // }), push_res);
    // if (push_res) {
    //     result = push_res;
    //     goto error;
    // }

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

    int posix_res = add_posix_stuff(builder, group_id);
    if (posix_res) {
        result = posix_res;
        goto error;
    }
    int fs_res = pass_filesystem(builder, page_table_id, group_id);
    if (fs_res) {
        result = fs_res;
        goto error;
    }

    // // Stack stuff
    size_t auxvec_size = 0;
    int serial_result = auxvec_serialize(builder, stack_result.virt_addr_intptr + stack_size, &auxvec_data, &auxvec_size);
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


    uint64_t actual_entry;
    if (interp_mapped) {
        actual_entry = interp_entry + interp_rel_offset;
    } else {
        actual_entry = program_entry + program_rel_offset;
    }
    auto start_result = syscall_start_process(task_id, actual_entry, 0, 0, 0);
    if (start_result) {
        result = start_result;
        goto error;
    }

error:
    free(auxvec_data);
    auxvec_free(builder);
    if (interp_mapped)
        release_region(0, interp_mapped);
    if (file_mapped)
        release_region(0, file_mapped);

    return result;
}