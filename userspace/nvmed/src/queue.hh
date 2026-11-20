#pragma once
#include <cstdint>
#include <memory>
#include "common.hh"
#include <pmos/containers/intrusive_list.hh>
#include <pmos/async/coroutines.hh>

struct NVMeCompletionEntry {
    uint32_t command_specific;
    uint32_t reserved;
    uint16_t sq_head_pointer;
    uint16_t sq_identifier;
    uint16_t command_id;
    uint16_t status;

    enum class CodeType : uint16_t {
        Generic = 0x0,
        CommandSpecific = 0x1,
        MediaError = 0x2,
        Path = 0x3,
        VendorSpecific = 0x7,
    };

    uint16_t status_code() const
    {
        return (status >> 1) & 0xff;
    }

    CodeType status_code_type() const
    {
        return (CodeType)((status >> 9) & 0x7);
    }

    bool success() const
    {
        return status_code_type() == CodeType::Generic && status_code() == 0;
    }
};

struct NVMeCommand {
    uint8_t opcode;
    uint8_t flags;
    uint16_t command_id;
    uint32_t nsid;
    uint64_t reserved1;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
};

constexpr uint32_t SUBMISSION_QUEUE_ENTRY_SIZE = 64;
constexpr uint32_t COMPLETION_QUEUE_ENTRY_SIZE = 16;

struct NVMeSubmissionQueue;

struct NVMeCompletionQueue {
    void *dma_virt_base;
    unsigned size;
    bool phase = true;

    uint32_t queue_head = 0;
    uint32_t queue_tail = 0;

    uint32_t index = 0;
    void head_doorbell_write(uint32_t value)
    {
        nvme_write32(NVME_REG_SQ0TDBL + index * (8 << doorbell_stride) + (4 << doorbell_stride), value);
    }

    struct WaitForCompletion {
        inline bool await_ready() noexcept
        {
            return false;
        }
        inline void await_suspend(std::coroutine_handle<> h)
        {
            this->h = h;
            queue.waiters.push_back(this);
        }
        inline NVMeCompletionEntry await_resume() noexcept
        {
            return result;
        }

        std::coroutine_handle<> h;
        NVMeCompletionQueue &queue;
        pmos::containers::DoubleListHead<WaitForCompletion> node;
        uint16_t command;
        NVMeCompletionEntry result;
        WaitForCompletion(NVMeCompletionQueue &queue, uint16_t command):
            queue(queue), command(command)
        {}
    };

    pmos::containers::CircularDoubleList<WaitForCompletion, &WaitForCompletion::node> waiters;

    void check_for_completions();

    NVMeSubmissionQueue *submission_queue = nullptr;
};

struct NVMeSubmissionQueue {
    std::shared_ptr<NVMeCompletionQueue> completion_queue;
    void *dma_virt_base = {};
    unsigned size = {};

    uint32_t queue_head = {};
    uint32_t queue_tail = {};

    inline bool is_full() const
    {
        return ((queue_tail + 1) & (size - 1)) == queue_head;
    }

    pmos::async::task<NVMeCompletionEntry> submit_command(NVMeCommand &cmd);

    uint32_t index = 0;

    void tail_doorbell_write(uint32_t value)
    {
        nvme_write32(NVME_REG_SQ0TDBL + index * (8 << doorbell_stride), value);
    }

    struct WaitForQueue {
        inline bool await_ready() noexcept
        {
            return false;
        }
        inline void await_suspend(std::coroutine_handle<> h)
        {
            this->h = h;
            queue.waiters.push_back(this);
        }
        inline void await_resume() noexcept
        {}

        std::coroutine_handle<> h;
        NVMeSubmissionQueue &queue;
        pmos::containers::DoubleListHead<WaitForQueue> node;
        WaitForQueue(NVMeSubmissionQueue &queue):
            queue(queue)
        {}
    };

    pmos::containers::CircularDoubleList<WaitForQueue, &WaitForQueue::node> waiters;

    void wake_waiters();
};