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

#ifndef _PMOS_MEMORY_H
#define _PMOS_MEMORY_H
#include "memory_flags.h"
#include "system.h"

#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef TASK_ID_SELF
    /// Parameter used in some function calls to indicate the current tasks PID should be used
    #define TASK_ID_SELF 0
#endif

/***
 * A structure returned by the memory-related system calls
 */
typedef struct mem_request_ret_t {
    result_t result; ///< The result of the operation
    void *virt_addr; ///< Address of the new region. Does not hold a meningful value if the result
                     ///< was not successfull.
} mem_request_ret_t;

/// pmOS memory object identificator
typedef unsigned long mem_object_t;

#define SEGMENT_FS 1
#define SEGMENT_GS 2
#define GENERAL_REGS 3

struct task_register_set {
    uint64_t pc;
    uint64_t ra;
    uint64_t sp;
    uint64_t gp;
    uint64_t tp;
    uint64_t t0;
    uint64_t t1;
    uint64_t t2;
    uint64_t s0;
    uint64_t s1;
    uint64_t a0;
    uint64_t a1;
    uint64_t a2;
    uint64_t a3;
    uint64_t a4;
    uint64_t a5;
    uint64_t a6;
    uint64_t a7;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
    uint64_t t3;
    uint64_t t4;
    uint64_t t5;
    uint64_t t6;
};

#define CREATE_FLAG_FIXED 0x08

#ifdef __STDC_HOSTED__
/// @brief Creates a normal page region
///
/// This syscall creates a memory region with no backing objects, initialized to 0. The functioning
/// depends on the kernel, but normally the page allocation would be delayed untill the memory is
/// actually accessed. The memory can be safely unallocated using release_region() syscall
///
/// @param pid PID of the process holding where the region should be allocated. Takes TASK_ID_SELF
/// (0)
/// @param addr_start The suggestion for the virutal address of the new region. The parameter must
/// be page-alligned,
///                   otherwise will ignore it (as if NULL was passed). If the address is
///                   not occupied, the kernel will try and place the new region there. Otherwise,
///                   the behaviour depends on the *access* arguments, where either the new location
///                   would be found or the error would be returned.
/// @param size The size in bytes of the new region. The size must be page-alligned and not 0,
/// otherwise the error will be returned
/// @param access An OR-conjugated list of the argument. Takes PROT_READ, PROT_WRITE and PROT_EXEC
/// as access bytes and CREATE_FLAG_FIXED
///               if addr_start should always be obeyed.
/// @returns mem_request_ret_t structure. Result indicated if the operation was successfull and
/// error otherwise. If the operation was successfull,
///          virt_addr contains the address of the new virtual region.
/// @see release_region()
mem_request_ret_t create_normal_region(uint64_t pid, void *addr_start, size_t size,
                                       uint64_t access);

/**
 * @brief Create a physically mapped memory region. The functioning is very similar to
 * create_normal_region() with the exception that when accessing it, the physical location indicated
 * by *phys_addr* would be references.
 * @param phys_addr Physical address that the new region would be mapped to. The address must be
 * page aligned, otherwise the error would be returned.
 * @see create_normal_region()
 */
mem_request_ret_t create_phys_map_region(uint64_t pid, void *addr_start, size_t size,
                                         uint64_t access, void *phys_addr);

/**
 * @brief Maps a memory object to the new region.
 * @param page_table_id ID of the page table where the new region should be created. Takes
 * PAGE_TABLE_SELF (0) for the current process.
 * @param addr_start The suggestion for the virutal address of the new region. The parameter must be
 * page-alligned.
 * @param size The size in bytes of the new region. The size must be page-alligned and not 0,
 * otherwise the error will be returned
 * @param access An OR-conjugated list of the argument. Takes PROT_READ, PROT_WRITE and PROT_EXEC as
 * access bytes and CREATE_FLAG_FIXED, and FLAG_COW
 * @param object_id ID of the memory object that should be mapped to the new region.
 * @param offset Offset in the memory object where the mapping should start. The offset must be
 * page-alligned.
 * @returns mem_request_ret_t structure. Result indicated if the operation was successfull and error
 * otherwise. If the operation was successfull, virt_addr contains the address of the new virtual
 * region.
 */
mem_request_ret_t map_mem_object(uint64_t page_table_id, void *addr_start, size_t size,
                                 uint64_t access, mem_object_t object_id, size_t offset);

/**
 * @brief Transfers a memory region to the new page table.
 *
 * This system call transfers the given region to the new process, conserving its size and
 * properties. This function is very similar to create_*_region in the arguments, except that the
 * pages are taken out of the current process and are spawned in the new process.
 *
 * @param to_page_table Destination Page Table object. The table can be used by a running process.
 * @param region The start address of the region inside the current process
 * @param dest The hint for the kernel of where to place the new region. See addr_start in
 * create_normal_region()
 * @param flags New access flags for the resulting memory region in the new page_table. Basically
 * same as access in create_normal_region(). See its documentation for more info.
 * @return Result of the operation and the resulting address in *to_page_table*. The latter is
 * meaningless if the result is not SUCCESS
 * @see create_normal_region()
 */
mem_request_ret_t transfer_region(uint64_t to_page_table, void *region, void *dest, uint64_t flags);

/// @brief Releases memory region
/// @param pid PID of the process holding the region that should be released. Takes TASK_ID_SELF (0)
/// @param region The start of the region that should be released.
/// @return SUCCESS if successfull, generic error otherwise
result_t release_region(uint64_t pid, void *region);

result_t release_memory_range(uint64_t pid, void *start, size_t len);

typedef uint64_t pmos_pagetable_t;
typedef struct page_table_req_ret_t {
    result_t result;
    pmos_pagetable_t page_table;
} page_table_req_ret_t;

/**
 * @brief Get the kernel page table object id for the process identified by PID.
 *        The process must have some page table asigned, otherwise an error will be returned.
 * @param pid The PID of the process. Can take TASK_ID_SELF (0)
 * @return page_table_req_ret_t has the result and the page table. If the result is not SUCCESSm
 * then page_table does not hold a meaningful value.
 */
page_table_req_ret_t get_page_table(uint64_t pid);

/**
 * @brief Sets the segment registers of the task indicated by PID. If setting for the other process,
 *        it must not be running at the time. Otherwise, the behaviour is undefined.
 *
 * @param pid The PID of the target task. Can take TASK_ID_SELF (0).
 * @param register_set The desired registers, defined by SEGMENT_FS and SEGMENT_GS and GENERAL_REGS constants
 * @param addr The pointer to the segment, or to the array holding the registers.
 * @return result_t The result of the operation.
 */
result_t set_registers(uint64_t pid, unsigned register_set, void *addr);

/**
 * @brief Gets the registers of the task indicated by PID. If getting for the other process,
 *        it must not be running at the time. Otherwise, the behaviour is undefined. Requesting the
 *        segment for self is always allowed.
 *
 * @param pid The PID of the target task. Can take TASK_ID_SELF (0).
 * @param register_set The desired set of registers, defined by SEGMENT_FS and SEGMENT_GS and GENERAL_REGS constants
 * @param addr The pointer to the segment, or to the array where the registers should be stored.
 * @return syscall_r The result of the operation. If the result is SUCCESS, the value of the segment
 * is stored in the *value* field.
 */
result_t get_registers(uint64_t pid, unsigned register_set, void *addr);

    #define PAGE_TABLE_SELF 0

    #define PAGE_TABLE_CREATE 1
    #define PAGE_TABLE_ASIGN  2
    #define PAGE_TABLE_CLONE  3
/**
 * @brief Asigns a page table to the process
 *
 * This system call asigns the page table to the newly created process. After issuing the
 * start_process() function, the new process is found in an uninit state and has no page table. This
 * function can be either used to create a new page table or asign the existing one (e.g. for the
 * threads), in which case several process can share the same address space.
 *
 * Before execution, the process must not have a page table asigned. Flags can be used to define if
 * the new page table shall be created or inherited from *page_table* parameter.
 * @param pid PID of the target task, to which the page table must be asigned.
 * @param page_table ID of the page table if it is to be asigned. Given page table must not be
 * virtual. PAGE_TABLE_SELF might be used as a shorthand for inheriting the page table from the
 * caller. If PAGE_TABLE_CREATE is used, this parameter is ignored.
 * @param flags Flags defining the behaviour of the system call. Can take one of the following
 * values: PAGE_TABLE_CREATE - create a new empty page table. In this case, the page_table parameter
 * is ignored. PAGE_TABLE_ASIGN - asigns the page table provided by the page_table argument. In this
 * case, all the process with the same page table object share the same address space with the same
 * protections. PAGE_TABLE_CLONE - clones the page table provided by the page_table argument. In
 * this case, the page table is copied and the new process has its own address space. The page table
 * can be modified without affecting the other processes.
 * @return page_table_req_ret_t returns the result of the execution and the ID of the page table
 * asigned to the process. If result != SUCCESS, page_table does not hold a meaningful value.
 */
page_table_req_ret_t asign_page_table(uint64_t pid, uint64_t page_table, uint64_t flags);

/**
 * @brief Initializes stack for the given task
 *
 * This syscall initializes the stack for the task, by setting %rsp to the given value. The task
 * must be uninitialized, otherwise the behaviour is undefined. Apart from allocating and setting
 * the registers, the kernel does not manage the stack in any way. If reimplementing program
 * startup, threads and related, the task must take care of the stack in the userspace.
 *
 * If NULL is passed as a stack pointer, the kernel will allocate 2GB stack for the task. This
 * currently is hardcoded and needs to be revised in the future.
 * @param tid ID of the task
 * @param stack_top The top of the stack. If NULL, the kernel will allocate 2GB stack for the task.
 * @return syscall_r The result of the operation. If the result is SUCCESS, the pointer to the top
 * of the stack is stored in the *value* field.
 * @todo This functionality can be replicated in by the callee, both being more convenient,
 * flexible, faster (not requiring trip to kernel) and more akin to the pmOS philosophy. This
 * syscall should be revised in the future.
 */
syscall_r init_stack(uint64_t tid, void *stack_top);

#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif