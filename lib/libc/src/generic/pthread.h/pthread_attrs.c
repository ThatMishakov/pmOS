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

#include <errno.h>
#include <pthread.h>

int pthread_attr_destroy(pthread_attr_t *) { return 0; }

int pthread_attr_getdetachstate(const pthread_attr_t *attrs_ptr, int *detachstate)
{
    if (!attrs_ptr || !detachstate) {
        errno = EINVAL;
        return -1;
    }

    *detachstate = attrs_ptr->detachstate;
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{
    if (!attr) {
        errno = EINVAL;
        return -1;
    }

    switch (detachstate) {
    case PTHREAD_CREATE_JOINABLE:
    case PTHREAD_CREATE_DETACHED:
        attr->detachstate = detachstate;
        break;
    default:
        // errno = EINVAL;
        return -1;
        break;
    }

    return 0;
}

int pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize)
{
    if (!attr || !guardsize) {
        errno = EINVAL;
        return -1;
    }

    attr->guardsize = guardsize;
    return 0;
}

int pthread_attr_getguardsize(const pthread_attr_t *attr, size_t *guardsize)
{
    if (!attr || !guardsize) {
        errno = EINVAL;
        return -1;
    }

    *guardsize = attr->guardsize;
    return 0;
}