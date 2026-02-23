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

impl RightRequestResult {
    pub fn result(self: RightRequestResult) -> Result<Right, Error> {
        self.result.result().map(|()| self.right)
    }
}

unsafe extern "C" {
    fn create_port(owner_pid: libc::pid_t, flags: u32) -> PortReqResult;
    fn pmos_delete_port(port: Port) -> ResultT;
    fn syscall_get_message_info(descr: &mut MessageDescriptor, port: u64, flags: u32) -> ResultT;
    unsafe fn get_first_message(buff: *mut u8, args: u32, port: u64) -> RightRequestResult;
    #[link_name = "name_right"]
    unsafe fn name_right_stdlib(right: Right, name: *const libc::c_char, length: libc::size_t, flags: u32) -> ResultT;
    unsafe fn delete_right(id: Right) -> ResultT;
    unsafe fn dup_right(id: Right) -> RightRequestResult;
    unsafe fn create_right(port: Port, id_in_reciever: *mut Right, flags: u32) -> RightRequestResult;
    unsafe fn accept_rights(port: Port, rights_array: *mut u64) -> ResultT;
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

const MESSAGE_FLAG_HAS_REPLY_RIGHT: u32 = 1 << 0;
const MESSAGE_FLAG_REPLY_RIGHT_SEND_MANY: u32 = 1 << 1; // SendOnce otherwise...
const MESSAGE_FLAG_RIGHT0_SEND_MANY: u32 = 1 << 16;
const MESSAGE_FLAG_RIGHT1_SEND_MANY: u32 = 1 << 17;
const MESSAGE_FLAG_RIGHT2_SEND_MANY: u32 = 1 << 18;
const MESSAGE_FLAG_RIGHT3_SEND_MANY: u32 = 1 << 19;

const MESSAGE_FLAG_RIGHT0_SEND_MANY_SHIFT: usize = 16;

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
            sender_object_id: 0,
            sent_with_right: 0,
            other_rights_count: 0,
            flags: 0,
        };

        unsafe { syscall_get_message_info(&mut desc, self.port, 0) }
            .result()
            .unwrap();

        let mut other_rights = [const { None }; 4];
        if desc.other_rights_count > 0 {
            let mut other_rights_raw = [0u64; 4];
            unsafe { accept_rights(self.port, other_rights_raw.as_mut_ptr()) }
                .result()
                .unwrap();

            for i in 0..4 {
                let rid = other_rights_raw[i];
                if rid != 0 {
                    other_rights[i] = Some(if desc.flags & (1 << (MESSAGE_FLAG_RIGHT0_SEND_MANY_SHIFT + i)) != 0 {
                        SendRight::Many(SendManyRight(rid))
                    } else {
                        SendRight::Once(SendOnceRight(rid))
                    })
                }
            }
        }

        let mut buffer = vec![0u8; desc.size as usize];
        let r = unsafe { get_first_message(buffer.as_mut_ptr(), 0, self.port) }
            .result()
            .unwrap();

        let right= if r != 0 {
            if desc.flags & MESSAGE_FLAG_REPLY_RIGHT_SEND_MANY != 0 {
                Some(SendRight::Many(SendManyRight(r)))
            } else {
                Some(SendRight::Once(SendOnceRight(r)))
            }
        } else {
            None
        };

        Message {
            data: buffer.into_boxed_slice(),
            sender: desc.sender,
            object: MemoryObject::from_message(desc.mem_object),
            sent_with_right: desc.sent_with_right,
            reply_right: right,
            other_rights,
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
    sender_object_id: u64,
    sent_with_right: u64,
    other_rights_count: u32,
    flags: u32,
}

pub struct Message {
    pub data: Box<[u8]>,
    pub sender: u64,
    pub sent_with_right: u64,
    pub object: Option<MemoryObject>,
    pub reply_right: Option<SendRight>,
    pub other_rights: [Option<SendRight>; 4],
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

pub enum SendRight {
    Once(SendOnceRight),
    Many(SendManyRight)
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

impl From<SendManyRight> for SendRight {
    fn from(right: SendManyRight) -> SendRight {
        SendRight::Many(right)
    }
}

impl From<SendOnceRight> for SendRight {
    fn from(right: SendOnceRight) -> SendRight {
        SendRight::Once(right)
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
