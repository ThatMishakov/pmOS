use super::system::ResultT;
use core::ffi::c_uint;
use std::num::NonZero;

pub struct MemoryObject(NonZero<u64>);

extern "C" {
    fn release_mem_object(object_id: u64, flags: c_uint) -> ResultT;
}

impl MemoryObject {
    pub(super) fn from_message(mem_object_id: u64) -> Option<MemoryObject> {
        NonZero::<u64>::new(mem_object_id).map(MemoryObject)
    }

    pub fn id(&self) -> u64 {
        self.0.get()
    }
}

impl Drop for MemoryObject {
    fn drop(&mut self) {
        unsafe { release_mem_object(self.0.get(), 0).result() }.unwrap()
    }
}
