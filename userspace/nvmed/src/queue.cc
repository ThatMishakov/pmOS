#include "queue.hh"
#include <pmos/device_handler.hh>
#include <memory>
#include <optional>
#include <cassert>

void NVMeSubmissionQueue::wake_waiters()
{
    while (!waiters.empty() && !is_full()) {
        auto &waiter = waiters.front();
        waiters.erase(&waiter);
        waiter.h.resume();
    }
}

pmos::async::task<NVMeCompletionEntry> NVMeSubmissionQueue::submit_command(NVMeCommand &cmd)
{
    const auto mask = size - 1;

    uint32_t tail;
    uint32_t next;
    while (true) {
        tail = queue_tail;
        next = (tail + 1) & mask;

        if (next != queue_head)
            break;

        co_await WaitForQueue(*this);
    }

    memcpy((uint8_t *)dma_virt_base + tail * sizeof(NVMeCommand), &cmd, sizeof(NVMeCommand));
    pmos::dma_wmb();

    queue_tail = next;
    tail_doorbell_write(next);

    co_return co_await NVMeCompletionQueue::WaitForCompletion(*completion_queue, cmd.command_id);
}

void NVMeCompletionQueue::check_for_completions()
{
    const auto mask = size - 1;
    std::optional<uint32_t> submission_queue_head;

    pmos::dma_rmb();

    while (true) {
        auto entry = (NVMeCompletionEntry *)((uint8_t *)dma_virt_base + queue_head * sizeof(NVMeCompletionEntry));
        
        auto status = dma_read16(entry->status);
        if ((status & 1) != phase)
            break;

        pmos::dma_rmb();

        assert(!waiters.empty());
        auto &waiter = waiters.front();
        waiters.erase(&waiter);
        memcpy(&waiter.result, entry, sizeof(NVMeCompletionEntry));
        submission_queue_head = entry->sq_head_pointer;

        assert(waiter.command == waiter.result.command_id);

        waiter.h.resume();

        queue_head = (queue_head + 1) & mask;
        if (queue_head == 0)
            phase = !phase;
    }

    if (submission_queue_head) {
        head_doorbell_write(queue_head);

        assert(submission_queue);
        submission_queue->queue_head = *submission_queue_head;
        submission_queue->wake_waiters();
    }

    nvme_write32(NVME_REG_INTMC, 1);
}