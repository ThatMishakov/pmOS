use super::error::Error;
use super::mem_object::MemoryObject;
use super::system::ResultT;
use core::ffi::c_uint;
use std::mem;

pub struct IPCPort {
    port: Port,
}

pub type Port = u64;
pub type Right = u64;

#[repr(C)]
struct PortReqResult {
    result: ResultT,
    port: Port,
}

#[repr(C)]
struct RightRequestResult {
    result: ResultT,
    right: Right,
}

unsafe extern "C" {
    fn create_port(owner_pid: libc::pid_t, flags: u32) -> PortReqResult;
    fn pmos_delete_port(port: Port) -> ResultT;
    fn syscall_get_message_info(descr: &mut MessageDescriptor, port: u64, flags: u32) -> ResultT;
    unsafe fn get_first_message(buff: *mut u8, args: u32, port: u64) -> ResultT;
    #[link_name = "name_right"]
    unsafe fn name_right_stdlib(right: Right, name: *const libc::c_char, length: libc::size_t, flags: u32) -> ResultT;
    unsafe fn delete_right(id: Right) -> ResultT;
    unsafe fn dup_right(id: Right) -> RightRequestResult;
    unsafe fn create_right(port: Port, id_in_reciever: *mut Right, flags: u32) -> RightRequestResult;
}

impl Drop for IPCPort {
    fn drop(&mut self) {
        let result = unsafe {
            pmos_delete_port(self.port)
        };

        if !result.success() {
            panic!("couldn't delete port")
        }
    }
}

const CREATE_RIGHT_SEND_ONCE: u32 = 1 << 0;

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

    pub fn create_right_sendonce(& self) -> Option<(SendOnceRight, RecieveRight)> {
        let mut recieve_id: Right = 0;
        let RightRequestResult { result, right } = unsafe { create_right(self.port, &raw mut recieve_id, CREATE_RIGHT_SEND_ONCE) };

        if result.success() {
            Some((SendOnceRight(right), RecieveRight(recieve_id)))
        } else {
            None
        }
    }

    pub fn create_right_sendmany(& self) -> Option<(SendManyRight, RecieveRight)> {
        let mut recieve_id: Right = 0;
        let RightRequestResult { result, right } = unsafe { create_right(self.port, &raw mut recieve_id, 0) };

        if result.success() {
            Some((SendManyRight(right), RecieveRight(recieve_id)))
        } else {
            None
        }
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

pub fn send_message(
    msg: &impl super::ipc_msgs::Serializable,
    port: u64,
    object: Option<MemoryObject>,
) -> Result<(), Error> {
    let data = msg.serialize();
    let size = data.len();
    if let Some(object) = object {
        unsafe { send_message_port2(port, object.id(), size, data.as_ptr(), 0) }.result()?;
        mem::forget(object);
    } else {
        unsafe { send_message_port(port, size, data.as_ptr()) }.result()?
    }
    Ok(())
}

impl Message {
    pub fn get_known_id(&self) -> Option<u32> {
        Some(u32::from_ne_bytes(self.data.get(0..4)?.try_into().ok()?))
    }
}

pub struct SendManyRight (Right);
pub struct SendOnceRight (Right);
pub struct RecieveRight (Right);
// TODO: Destructor for this

impl SendManyRight {
    fn new(id: Right) -> Self {
        Self(id)
    }

    pub fn clone(&self) -> Result<Self, Error> {
        let RightRequestResult { result, right } = unsafe { dup_right(self.0) };
        if result.success() {
            Ok(Self::new(right))
        } else {
            Err(result.result().unwrap_err())
        }
    }

    pub fn name_self(self, name: &str) -> Result<(), Error> {
        unsafe {
            name_right_stdlib(self.0, name.as_ptr() as *const libc::c_char, name.len(), 0)
        }.result().map(|_| mem::forget(self))
    }
}

impl Drop for SendManyRight {
    fn drop(&mut self) {
        unsafe {
            _ = delete_right(self.0)
        }
    }
}

impl Drop for SendOnceRight {
    fn drop(&mut self) {
        unsafe {
            _ = delete_right(self.0)
        }
    }
}
