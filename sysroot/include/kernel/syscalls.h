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

#ifndef KERNEL_SYSCALLS_H
#define KERNEL_SYSCALLS_H

#define PMOS_SYSCALL_INT 0xf8

#define SYSCALL_EXIT             0
#define SYSCALL_GET_TASK_ID      1
#define SYSCALL_CREATE_PROCESS   2
#define SYSCALL_START_PROCESS    3
#define SYSCALL_INIT_STACK       4
#define SYSCALL_SET_PRIORITY     5
#define SYSCALL_SET_TASK_NAME    6
#define SYSCALL_GET_LAPIC_ID     7
#define SYSCALL_CONFIGURE_SYSTEM 8

#define SYSCALL_GET_MSG_INFO       9
#define SYSCALL_GET_MESSAGE        10
#define SYSCALL_REQUEST_NAMED_PORT 11
#define SYSCALL_SEND_MSG_PORT      12
#define SYSCALL_CREATE_PORT        13
#define SYSCALL_SET_ATTR           14
#define SYSCALL_SET_INTERRUPT      15
#define SYSCALL_NAME_PORT          16
#define SYSCALL_GET_PORT_BY_NAME   17
#define SYSCALL_SET_LOG_PORT       18

#define SYSCALL_GET_PAGE_TABLE 19

#define SYSCALL_TRANSFER_REGION      21
#define SYSCALL_CREATE_NORMAL_REGION 22
#define SYSCALL_GET_REGISTERS        23
#define SYSCALL_CREATE_PHYS_REGION   24
#define SYSCALL_DELETE_REGION        25
#define SYSCALL_UNMAP_RANGE          26
#define SYSCALL_MEM_PROTECT          27
#define SYSCALL_SET_REGISTERS        28
#define SYSCALL_ASIGN_PAGE_TABLE     29
#define SYSCALL_CREATE_MEM_OBJECT    30

#define SYSCALL_CREATE_TASK_GROUP      31
#define SYSCALL_ADD_TASK_TO_GROUP      32
#define SYSCALL_REMOVE_TASK_FROM_GROUP 33
#define SYSCALL_CHECK_IF_TASK_IN_GROUP 34

#define SYSCALL_SET_NOTIFY_MASK 35
#define SYSCALL_LOAD_EXECUTABLE 36

#define SYSCALL_REQUEST_TIMER      37
#define SYSCALL_SET_AFFINITY       38
#define SYSCALL_COMPLETE_INTERRUPT 39
#define SYSCALL_YIELD              40
#define SYSCALL_MAP_MEM_OBJECT     41
#define SYSCALL_UNSET_INTERRUPT    42
#define SYSCALL_GET_TIME           43
#define SYSCALL_GET_SYSTEM_INFO    44
#define SYSCALL_KILL_TASK          45
#define SYSCALL_PAUSE_TASK         46
#define SYSCALL_RESUME_TASK        47

#define SYSCALL_GET_PAGE_ADDRESS            48
#define SYSCALL_RELEASE_MEM_OBJECT          49
#define SYSCALL_MEM_OBJECT_GET_PAGE_ADDRESS 50

#endif