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

#include <interrupts/interrupts.h>
#include <vector.h>
#include <sys/sysinfo.h>
#include <pmos/system.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

unsigned char interrupt_number = 245;

uint8_t get_free_interrupt() { return interrupt_number--; }

struct CPUDescriptor {
    uint32_t lapic_id;
    int interrupt_number;
};

VECTOR(struct CPUDescriptor) cpus;

void init_cpus()
{
    int count = get_nprocs();
    assert(count > 0);

    for (int i = 0; i < count; ++i) {
        syscall_r a = get_lapic_id(i + 1);
        if (a.result != 0) {
            fprintf(stderr, "Warning: Could not get LAPIC ID for CPU %i: %li (%s)\n", i, a.result, strerror(-a.result));
            continue;
        }

        int r = 0;
        struct CPUDescriptor d = {
            .lapic_id = (uint32_t)a.value,
            .interrupt_number = 245,
        };

        VECTOR_PUSH_BACK_CHECKED(cpus, d, r);
        if (r != 0) {
            fprintf(stderr, "Warning: Could not push CPU to vector: %i (%s)\n", errno, strerror(errno));
        }
    }
}

int assign_int_vector(int *cpu_id_out, uint8_t *vector_out)
{
    if (cpus.size == 0) {
        // This wasn't initialized??
        fprintf(stderr, "Error: Could not assign new interrupt vector, no CPUs are online\n");
        return -EINVAL;
    } 

    // Get a random CPU
    int id = rand() % cpus.size;
    struct CPUDescriptor *d = cpus.data + id;
    if (d->interrupt_number < 48)
        return -ENOMEM;

    *cpu_id_out = id;
    *vector_out = d->interrupt_number--;
    return 0;
}