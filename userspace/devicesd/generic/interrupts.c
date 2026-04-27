#include <interrupts.h>
#include <pmos/system.h>
#include <pmos/interrupts.h>
#include <errno.h>
#include <pmos/ipc.h>
#include <string.h>
#include <inttypes.h>

void request_interrupts_for(Message_Descriptor *desc, IPC_Request_Int *m, pmos_right_t reply_right)
{
    message_extra_t extra = {0};
    uint32_t gsi = 0;
    bool active_low = false;
    bool level_trig = false;
    int result = 0;

    if (desc->size < sizeof(IPC_Request_Int)) {
        fprintf(stderr, "Error: IPC_Request_Int message too small! Size: %" PRIu64 "\n", desc->size);
        result = -EINVAL;
        goto end;
    }

    if (m->flags & IPC_Request_Int_FLAG_EXT_INTS) {
        int_redirect_descriptor desc = isa_gsi_mapping(m->intno);
        gsi = desc.destination;
        active_low = desc.active_low;
        level_trig = desc.level_trig;
    } else {
        gsi = m->intno;
        active_low = m->int_flags & INTERRUPT_FLAG_ACTIVE_LOW;
        level_trig = m->int_flags & INTERRUPT_FLAG_LEVEL_TRIGGERED;
    }

    uint32_t flags = 0;
    if (active_low)
        flags |= PMOS_INTERRUPT_ACTIVE_LOW;
    if (level_trig)
        flags |= PMOS_INTERRUPT_LEVEL_TRIG;

    right_request_t irq_right_r = allocate_interrupt(gsi, flags);
    if (irq_right_r.result != 0) {
        fprintf(stderr, "Failed to allocate interrupt for GSI %u: %i (%s)\n", gsi, (int)irq_right_r.result,
               strerror(-irq_right_r.result));

        result = irq_right_r.result;
        goto end;
    }
    extra.extra_rights[0] = irq_right_r.right;
    
end:
    IPC_Request_Int_Reply reply = {
        .type = IPC_Request_Int_Reply_NUM,
        .status = result,
    };
    result = send_message_right(reply_right, 0, &reply, sizeof(reply), &extra, 0).result;
    if (result < 0) {
        delete_right(reply_right);
        if (extra.extra_rights[0])
            delete_right(extra.extra_rights[0]);
        fprintf(stderr, "Warning could not reply to task %#" PRIx64 " port %#" PRIx64 " in configure_interrupts_for: %i (%s)\n", desc->sender, reply_right, result, strerror(-result));
    }
}