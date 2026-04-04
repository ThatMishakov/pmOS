use super::system::ResultT;
use bitflags::bitflags;
use super::error::Error;
use super::ipc::Port;

#[repr(C)]
pub(super) struct SyscallR {
    result: ResultT,
    value: u64,
}

extern "C" {
    fn is_task_group_member(task_id: u64, group_id: u64) -> SyscallR;
    fn set_task_group_notifier_mask(task_group_id: u64, port_id: u64, new_mask: u32, flags: u32) -> SyscallR;
}

pub fn is_task_of_group(task_id: u64, group_id: u64) -> bool {
    let result = unsafe {
        is_task_group_member(task_id, group_id)
    };

    result.result.success() && result.value == 1
}

pub fn set_group_notifier_mask(task_group_id: u64, port_id: Port, new_mask: NotificationMask, flags: u32) -> Result<NotificationMask, Error> {
    let result = unsafe {
        set_task_group_notifier_mask(task_group_id, port_id, new_mask.bits(), flags)
    };

    if result.result.success() {
        Ok(NotificationMask::from_bits_truncate(result.value as u32))
    } else {
        Err(Error::from_result_t(result.result))
    }
}

bitflags! {
    pub struct NotificationMask: u32 {
        const GROUP_DESTROYED = 0x01;
        const TASK_REMOVED = 0x02;
        const TASK_ADDED = 0x04;
    }
}