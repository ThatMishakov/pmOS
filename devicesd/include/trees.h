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

#ifndef TREES_H
#define TREES_H


#define rotate_left(root, node, field) \
    { \
        timer_tree* right = node->field.r; \
        node->field.r = right->field.l; \
        if (node->field.r) \
            node->field.r->field.p = node; \
        right->field.p = node->field.p; \
        if (!node->field.p) \
            root = right; \
        else if (node == node->field.p->field.l) \
            node->field.p->field.l = right; \
        else \
            node->field.p->field.r = right; \
        right->field.l = node; \
        node->field.p = right; \
    }

#define rotate_right(root, node, field) \
    { \
        timer_tree* left = node->field.l; \
        node->field.l = left->field.r; \
        if (node->field.l) \
            node->field.l->field.p = node; \
        left->field.p = node->field.p; \
        if (!node->field.p) \
            root = left; \
        else if (node == node->field.p->field.l) \
            node->field.p->field.l = left; \
        else \
            node->field.p->field.r = left; \
        left->field.l = node; \
        node->field.p = left; \
    }

#endif