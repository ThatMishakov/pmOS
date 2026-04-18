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

#pragma once

#include <array>
#include <lib/list.hh>
#include <lib/memory.hh>
#include <lib/queue.hh>
#include <lib/splay_tree_map.hh>
#include <lib/vector.hh>
#include <memory/rcu.hh>
#include <pmos/containers/intrusive_bst.hh>
#include <pmos/containers/intrusive_list.hh>
#include <pmos/containers/map.hh>
#include <pmos/containers/set.hh>
#include <types.hh>
#include <utils.hh>

namespace kernel::proc
{
class TaskDescriptor;
class TaskGroup;
}; // namespace kernel::proc

namespace kernel::ipc
{

struct Right;

using rights_array   = std::array<Right *, 4>;
using message_buffer = klib::vector<char>;

struct GenericMessage {
    pmos::containers::DoubleListHead<GenericMessage> list_node;
    virtual size_t size() const = 0;

    using rights_array = std::array<Right *, 4>;

    // This was added here returning nullptr here instead of making it a pure virtual function
    // so that there's less boilerplate in message types that don't hold a reply right
    virtual Right *get_reply_right() const;
    virtual void clear_reply_right();

    // Same here as with the reply right...
    virtual rights_array &get_rights();
    virtual const rights_array &get_rights() const;

    virtual u64 sent_with_right() const = 0;
    virtual u64 sender_task_id() const = 0;

    virtual inline size_t rights_count() const
    {
        size_t i = 0;
        for (auto r: get_rights())
            i += r != nullptr;
        return i;
    }

    virtual ReturnStr<bool> copy_to_user_buff(char *buff) const = 0;

    virtual ~GenericMessage() = default;
};

// The final here is more of an optimization, more than anything else, I might inherit from this later as well...
struct Message final: public GenericMessage {
    u64 task_id_from    = 0;
    u64 sent_with_right_ = 0;
    klib::vector<char> content;
    Right *reply_right            = {};
    std::array<Right *, 4> rights = {};

    Message(u64 task_id_from, klib::vector<char> content)
        : task_id_from(task_id_from), content(klib::move(content))
    {
    }

    virtual inline size_t size() const override { return content.size(); }

    // Returns true if done successfully, false otherwise (e.g. when syscall needs to be repeated)
    virtual ReturnStr<bool> copy_to_user_buff(char *buff) const override;

    virtual Right *get_reply_right() const override;
    virtual void clear_reply_right() override;

    virtual rights_array &get_rights() override;
    virtual const rights_array &get_rights() const override;

    virtual u64 sent_with_right() const override;
    virtual u64 sender_task_id() const override;

    virtual ~Message();
};

}; // namespace kernel::ipc