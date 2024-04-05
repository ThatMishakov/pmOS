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

#ifndef KERNEL_ERRORS_H
#define KERNEL_ERRORS_H

#define SUCCESS_REPEAT              1UL

#ifndef SUCCESS
#define SUCCESS                     0UL
#endif

#define ERROR_NOT_SUPPORTED        -1UL
#define ERROR_NOT_IMPLEMENTED      -2UL
#define ERROR_NO_PERMISSION        -3UL
#define ERROR_OUT_OF_RANGE         -4UL
#define ERROR_UNALLIGNED           -5UL
#define ERROR_PAGE_PRESENT         -6UL
#define ERROR_GENERAL              -7UL
#define ERROR_OUT_OF_MEMORY        -8UL
#define ERROR_HUGE_PAGE            -9UL
#define ERROR_PAGE_NOT_ALLOCATED   -10UL
#define ERROR_PAGE_NOT_PRESENT     -11UL
#define ERROR_WRONG_PAGE_TYPE      -12UL
#define ERROR_ALREADY_BLOCKED      -13UL
#define ERROR_NO_SUCH_PROCESS      -14UL
#define ERROR_PROCESS_INITED       -15UL
#define ERROR_CANT_MESSAGE_SELF    -16UL
#define ERROR_NO_MESSAGES          -17UL
#define ERROR_PORT_DOESNT_EXIST    -18UL
#define ERROR_PORT_CLOSED          -19UL
#define ERROR_PAGE_NOT_SHARED      -20UL
#define ERROR_NOT_PAGE_OWNER       -21UL
#define ERROR_ALREADY_PAGE_OWNER   -22UL
#define ERROR_SPECIAL_PAGE         -23UL
#define ERROR_NODESTINATION        -24UL
#define ERROR_NAME_EXISTS          -25UL
#define ERROR_PROTECTION_VIOLATION -26UL
#define ERROR_REGION_OCCUPIED      -27UL
#define ERROR_NO_FREE_REGION       -28UL
#define ERROR_HAS_PAGE_TABLE       -29UL
#define ERROR_HAS_NO_PAGE_TABLE    -30UL
#define ERROR_OBJECT_DOESNT_EXIST  -31UL
#define ERROR_NOT_MANAGED_REGION   -32UL
#define ERROR_BAD_FORMAT           -33UL
#define ERROR_INSTRUCTION_UNAVAILABLE -34UL
#define ERROR_BAD_INSTRUCTION     -35UL

#endif