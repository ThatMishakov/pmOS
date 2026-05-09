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
#include <types.hh>
#include "sched.hh"
#include <messaging/rights.hh>
#include <pmos/containers/intrusive_list.hh>

namespace kernel::ipc {

class Port;

}

namespace kernel::sched
{

struct TimerRight final: TimerNode, ipc::RecieveRight, AttentionNode {
    Spinlock lock;
    // The parent_cpu gets decided and set when the timer gets rearmed
    CPU_Info *parent_cpu = nullptr;
    
    u64 next_update_ns = 0;

    // This can either be in the message, or in the timers queue, but not both at the same time
    bool is_sent : 1 = false;
    bool in_timer_queue : 1 = false;
    bool waiting_for_attention : 1 = false;
    // Alive basically means it's in the parent's rights list
    bool alive : 1 = true;

    // TimerNode
    virtual void fire() override;

    // RecieveRight
    virtual bool destroy_recieve_right() override;
    virtual ipc::RightType recieve_type() const override;

    // GenericMessage overrides
    virtual void delete_self() override;
    
    virtual size_t size() const override;
    virtual ReturnStr<bool> copy_to_user_buff(char *buff) const override;

    static ReturnStr<TimerRight *> create_for_port(ipc::Port *port);

    // AttentionNode
    virtual void get_attention() override;

    kresult_t set_deadline(u64 new_deadline);
protected:
    bool should_delete_self() const;
    void request_attention();
};

} // namespace kernel::sched
