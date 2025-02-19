use super::error::Error;
use super::mem_object::MemoryObject;
use super::system::ResultT;
use core::ffi::c_uint;
use std::mem;

pub struct IPCPort {
    port: Port,
}

pub type Port = u64;

#[repr(C)]
struct PortReqResult {
    result: ResultT,
    port: Port,
}

extern "C" {
    fn create_port(owner_pid: libc::pid_t, flags: u32) -> PortReqResult;
    fn pmos_delete_port(port: Port) -> ResultT;
    fn syscall_get_message_info(descr: &mut MessageDescriptor, port: u64, flags: u32) -> ResultT;
    fn get_first_message(buff: *mut u8, args: u32, port: u64) -> ResultT;
    fn name_port(port: u64, name: *const libc::c_char, length: usize, flags: u32) -> ResultT;
}

impl Drop for IPCPort {
    fn drop(&mut self) {
        let result;
        unsafe {
            result = pmos_delete_port(self.port);
        }

        if !result.success() {
            panic!("couldn't delete port")
        }
    }
}

impl IPCPort {
    pub fn new() -> Option<IPCPort> {
        let result;
        unsafe {
            result = create_port(0, 0);
        }

        if !result.result.success() {
            None
        } else {
            Some(IPCPort { port: result.port })
        }
    }

    pub fn get_id(&self) -> Port {
        self.port
    }

    pub fn pop_front_blocking(&mut self) -> Message {
        let mut desc = MessageDescriptor {
            sender: 0,
            mem_object: 0,
            size: 0,
        };

        unsafe { syscall_get_message_info(&mut desc, self.port, 0) }
            .result()
            .unwrap();

        let mut buffer = vec![0u8; desc.size as usize];
        unsafe { get_first_message(buffer.as_mut_ptr(), 0, self.port) }
            .result()
            .unwrap();

        Message {
            data: buffer.into_boxed_slice(),
            sender: desc.sender,
            object: MemoryObject::from_message(desc.mem_object),
        }
    }

    pub fn name_port(&mut self, name: &str) {
        unsafe { name_port(self.port, name.as_ptr() as *const i8, name.len(), 0) }
            .result()
            .unwrap();
    }
}

#[repr(C)]
struct MessageDescriptor {
    sender: u64,
    mem_object: u64,
    size: u64,
}

pub struct Message {
    pub data: Box<[u8]>,
    pub sender: u64,
    pub object: Option<MemoryObject>,
}

extern "C" {
    fn send_message_port(port: u64, size: usize, message: *const u8) -> ResultT;
    fn send_message_port2(
        port: u64,
        object_id: u64,
        size: usize,
        message: *const u8,
        flags: c_uint,
    ) -> ResultT;
}

pub fn send_message<T>(msg: &T, port: u64, object: Option<MemoryObject>) -> Result<(), Error> {
    let size = mem::size_of::<T>();
    if let Some(object) = object {
        unsafe { send_message_port2(port, object.id(), size, msg as *const T as *const u8, 0) }
            .result()?;
        mem::forget(object);
    } else {
        unsafe { send_message_port(port, size, msg as *const T as *const u8 as *const u8) }
            .result()?
    }
    Ok(())
}

impl Message {
    pub fn get_known_id(&self) -> Option<u32> {
        Some(u32::from_ne_bytes(self.data.get(0..4)?.try_into().ok()?))
    }
}
