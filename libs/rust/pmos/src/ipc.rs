use super::error::Error;
use super::mem_object::MemoryObject;
use super::system::ResultT;
use core::ffi::c_uint;
use std::{cmp::Ordering, mem, ptr};
use std::assert_matches;
use std::ptr::NonNull;
use libc::c_int;

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

#[repr(C)]
struct SyscallR {
    result: ResultT,
    value: u64,
}

impl RightRequestResult {
    pub fn result(self: RightRequestResult) -> Result<Right, (Error, u64)> {
        self.result.result().map(|()| self.right).map_err(|r| (r, self.right))
    }
}

#[repr(C)]
struct SendRightAux {
    rights: [Right; 4],
}

impl SendRightAux {
    fn new() -> Self {
        Self {
            rights: [0; 4],
        }
    }
}

#[repr(C)]
#[derive(Debug)]
struct MapMemObjectParamT {
    page_table_id: u64,
    mem_object_id_right: Right,
    addr_start_uint: u64,
    size_uint: u64,
    offset_object: u64,
    offset_start: u64,
    object_size: u64,
    access_flags: u64,
}

unsafe extern "C" {
    fn create_port(owner_pid: libc::pid_t, flags: u32) -> PortReqResult;
    fn pmos_delete_port(port: Port) -> ResultT;
    fn syscall_get_message_info(descr: &mut MessageDescriptor, port: u64, flags: u32) -> ResultT;
    unsafe fn get_first_message(buff: *mut u8, args: u32, port: u64) -> RightRequestResult;
    #[link_name = "name_right"]
    unsafe fn name_right_stdlib(
        right: Right,
        name: *const libc::c_char,
        length: libc::size_t,
        flags: u32,
    ) -> ResultT;
    unsafe fn delete_right(id: Right) -> ResultT;
    unsafe fn dup_right(id: Right) -> RightRequestResult;
    unsafe fn create_right(
        port: Port,
        id_in_reciever: *mut Right,
        flags: u32,
    ) -> RightRequestResult;
    unsafe fn accept_rights(port: Port, rights_array: *mut u64) -> ResultT;
    #[link_name = "send_message_right"]
    unsafe fn send_message_right_stdlib(
        send_right: Right,
        reply_port: Port,
        message: *const u8,
        message_size: libc::size_t,
        aux_info: *const SendRightAux,
        flags: c_uint,
    ) -> RightRequestResult;

    unsafe fn delete_receive_right(
        port: Port,
        receive_right: Right,
    ) -> ResultT;

    #[link_name = "get_right_type"]
    unsafe fn get_right_type(right: Right) -> SyscallR;

    #[link_name = "munmap"]
    unsafe fn sys_munmap(addr: *mut libc::c_void, length: libc::size_t) -> c_int;

    unsafe fn map_mem_object(param: *const MapMemObjectParamT) -> SyscallR;
}

impl Drop for IPCPort {
    fn drop(&mut self) {
        let result = unsafe { pmos_delete_port(self.port) };

        if !result.success() {
            panic!("couldn't delete port")
        }
    }
}

const CREATE_RIGHT_SEND_ONCE: u32 = 1 << 0;

const MESSAGE_FLAG_HAS_REPLY_RIGHT: u32 = 1 << 0;
const MESSAGE_FLAG_REPLY_RIGHT_SEND_MANY: u32 = 1 << 1; // SendOnce otherwise...
// const MESSAGE_FLAG_RIGHT0_SEND_MANY: u32 = 1 << 16;
// const MESSAGE_FLAG_RIGHT1_SEND_MANY: u32 = 1 << 17;
// const MESSAGE_FLAG_RIGHT2_SEND_MANY: u32 = 1 << 18;
// const MESSAGE_FLAG_RIGHT3_SEND_MANY: u32 = 1 << 19;

const RIGHT_TYPE_SEND_ONCE: u32 = 1;
const RIGHT_TYPE_SEND_MANY: u32 = 2;
const RIGHT_TYPE_MEM_OBJECT: u32 = 3;

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
                    other_rights[i] = Some(
                        match (desc.flags >> (MESSAGE_FLAG_RIGHT0_SEND_MANY_SHIFT + i*4)) & 0xf {
                            RIGHT_TYPE_SEND_ONCE => SendRight::Once(SendOnceRight(rid)),
                            RIGHT_TYPE_SEND_MANY => SendRight::Many(SendManyRight(rid)),
                            RIGHT_TYPE_MEM_OBJECT => SendRight::Object(MemoryObjectRight(rid)),
                            _ => continue, // Invalid right type, ignore
                        }
                    )
                }
            }
        }

        let mut buffer = vec![0u8; desc.size as usize];
        let r = unsafe { get_first_message(buffer.as_mut_ptr(), 0, self.port) }
            .result()
            .unwrap();

        let right = if r != 0 {
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

    pub fn create_right_sendonce(&self) -> Result<(SendOnceRight, RecieveOnceRight), Error> {
        let mut recieve_id: Right = 0;
        let RightRequestResult { result, right } =
            unsafe { create_right(self.port, &raw mut recieve_id, CREATE_RIGHT_SEND_ONCE) };

        result.result().map(|()| (SendOnceRight(right), RecieveOnceRight(recieve_id, self.port)))
    }

    pub fn create_right_sendmany(&self) -> Result<(SendManyRight, RecieveManyRight), Error> {
        let mut recieve_id: Right = 0;
        let RightRequestResult { result, right } =
            unsafe { create_right(self.port, &raw mut recieve_id, 0) };

        result.result().map(|()| (SendManyRight(right), RecieveManyRight(recieve_id, self.port)))
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

const SEND_MESSAGE_DELETE_RIGHT: u32 = 1 << 8;
const REPLY_CREATE_SEND_MANY: u32 = 1 << 1;

// #[derive(Debug)]
// enum RightResult {
//     Once(SendOnceRight),
//     Many(SendManyRight),
//     None,
// }

#[derive(Debug)]
enum RecieveRightResult {
    Once(RecieveOnceRight),
    Many(RecieveManyRight),
    None,
}

fn send_message_right_internal(
    msg: &impl super::ipc_msgs::Serializable,
    right: &mut Option<SendRight>, // Do mut Option so that it can be taken out
    reply_port: Option<(&IPCPort, bool)>, /* create_send_many */
    include_rights: &mut [Option<SendRight>; 4],
) -> Result<RecieveRightResult, (Error, u64)> {
    let mut aux_rights_count = 0;
    let mut aux_struct = SendRightAux::new();

    for i in 0..4 {
        if let Some(r) = &include_rights[i] {
            aux_struct.rights[i] = r.get_id();
            aux_rights_count += 1;
        }
    }

    let aux_ptr = if aux_rights_count > 0 {
        &aux_struct
    } else {
        ptr::null()
    };

    let msg = msg.serialize();
    let msg_size = msg.len();
    let msg = msg.as_ptr();

    let send_many = reply_port.map_or(false, |(_, send_many)| send_many);

    let flags = reply_port.map_or(
        0,
        |(_, send_many)| if send_many { REPLY_CREATE_SEND_MANY } else { 0 },
    );

    let port_id = reply_port.map_or(
        0,
        |(p, _)| p.port,
    );

    let result = unsafe {
        send_message_right_stdlib(
            right.as_ref().ok_or_else(|| (Error::from_errno(libc::EINVAL), 0))?.get_id(),
            reply_port.map_or(0, |(i, _)| i.port),
            msg,
            msg_size,
            aux_ptr,
            flags,
        )
    }
    .result()?;

    match right.as_mut().unwrap() {
        SendRight::Once(_) => mem::forget(right.take()),
        SendRight::Many(_) => (),
        SendRight::Object(_) => mem::forget(right.take()),
        SendRight::Unknown(_) => mem::forget(right.take()),
    }    
    for i in include_rights {
        mem::forget(i.take());
    }

    if result != 0 {
        Ok(if send_many {
            RecieveRightResult::Many(RecieveManyRight(result, port_id))
        } else {
            RecieveRightResult::Once(RecieveOnceRight(result, port_id))
        })
    } else {
        Ok(RecieveRightResult::None)
    }
}

fn send_message_right_consume_internal(
    msg: &impl super::ipc_msgs::Serializable,
    right: SendRight,
    reply_port: Option<(&IPCPort, bool /* create send many */)>,
    include_rights: [Option<SendRight>; 4],
) -> Result<RecieveRightResult, (Error, u64)> {
    let mut aux_rights_count = 0;
    let mut aux_struct = SendRightAux::new();

    for i in 0..4 {
        if let Some(r) = &include_rights[i] {
            aux_struct.rights[i] = r.get_id();
            aux_rights_count += 1;
        }
    }

    let aux_ptr = if aux_rights_count > 0 {
        &aux_struct
    } else {
        ptr::null()
    };

    let msg = msg.serialize();
    let msg_size = msg.len();
    let msg = msg.as_ptr();

    let send_many = reply_port.map_or(false, |(_, send_many)| send_many);

    let flags = SEND_MESSAGE_DELETE_RIGHT
        | reply_port.map_or(
            0,
            |(_, send_many)| if send_many { REPLY_CREATE_SEND_MANY } else { 0 },
        );

    let port_id = reply_port.map_or(
        0,
        |(p, _)| p.port,
    );

    let result = unsafe {
        send_message_right_stdlib(
            right.get_id(),
            reply_port.map_or(0, |(i, _)| i.port),
            msg,
            msg_size,
            aux_ptr,
            flags,
        )
    }
    .result()?;

    mem::forget(right);
    for i in include_rights {
        mem::forget(i);
    }

    if result != 0 {
        Ok(if send_many {
            RecieveRightResult::Many(RecieveManyRight(result, port_id))
        } else {
            RecieveRightResult::Once(RecieveOnceRight(result, port_id))
        })
    } else {
        Ok(RecieveRightResult::None)
    }
}

pub fn send_message_right(
    msg: &impl super::ipc_msgs::Serializable,
    right: &mut Option<SendRight>,
    include_rights: &mut [Option<SendRight>; 4],
) -> Result<(), (Error, u64)> {
    send_message_right_internal(msg, right, None, include_rights).map(|_| ())
}

pub fn send_message_right_consume(
    msg: &impl super::ipc_msgs::Serializable,
    right: SendRight,
    include_rights: [Option<SendRight>; 4],
) -> Result<(), (Error, u64)> {
    send_message_right_consume_internal(msg, right, None, include_rights).map(|_| ())
}

impl Message {
    pub fn get_known_id(&self) -> Option<u32> {
        Some(u32::from_ne_bytes(self.data.get(0..4)?.try_into().ok()?))
    }
}

pub fn send_message_reply_once(
    msg: &impl super::ipc_msgs::Serializable,
    right: &mut Option<SendRight>,
    reply_port: &IPCPort,
    include_rights: &mut [Option<SendRight>; 4],
) -> Result<RecieveOnceRight, (Error, u64)> {
    let result = send_message_right_internal(msg, right, Some((reply_port, false)), include_rights)?;
    assert_matches!(result, RecieveRightResult::Once(_));
    if let RecieveRightResult::Once(r) = result {
        Ok(r)
    } else {
        unreachable!()
    }
}

pub enum SendRight {
    Once(SendOnceRight),
    Many(SendManyRight),
    Object(MemoryObjectRight),
    Unknown(UnknownRight),
}

pub fn send_right_from_id(id: Right) -> Result<SendRight, Error> {
    let result = unsafe { get_right_type(id) };
    result.result.result().and_then(|()| {
        match result.value as u32 {
            RIGHT_TYPE_SEND_ONCE => Ok(SendRight::Once(SendOnceRight(id))),
            RIGHT_TYPE_SEND_MANY => Ok(SendRight::Many(SendManyRight(id))),
            RIGHT_TYPE_MEM_OBJECT => Ok(SendRight::Object(MemoryObjectRight(id))),
            _ => Ok(SendRight::Unknown(UnknownRight(id))),
        }
    })
}

impl SendRight {
    pub fn get_id(&self) -> Right {
        match self {
            Self::Once(o) => o.0,
            Self::Many(o) => o.0,
            Self::Object(o) => o.0,
            Self::Unknown(o) => o.0,
        }
    }
}

impl Ord for SendRight {
    fn cmp(&self, other: &Self) -> Ordering {
        if self.get_id() < other.get_id() {
            Ordering::Less
        } else if self.get_id() > other.get_id() {
            Ordering::Greater
        } else {
            Ordering::Equal
        }
    }
}

impl PartialOrd for SendRight {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl PartialEq for SendRight {
    fn eq(&self, other: &Self) -> bool {
        self.get_id() == other.get_id()
    }
}

impl Eq for SendRight {}

#[derive(Debug)]
pub struct SendManyRight(Right);
#[derive(Debug)]
pub struct SendOnceRight(Right);
pub struct MemoryObjectRight(Right);
pub struct UnknownRight(Right);
#[derive(Debug)]
pub struct RecieveOnceRight(Right, Port);
#[derive(Debug)]
pub struct RecieveManyRight(Right, Port);
// TODO: Destructor for this

pub struct ObjectMmap{
    ptr: NonNull<u8>,
    size: usize,
}

impl ObjectMmap {
    pub fn as_slice(&self) -> &[u8] {
        unsafe { std::slice::from_raw_parts(self.ptr.as_ptr(), self.size) }
    }

    pub fn len(&self) -> usize {
        self.size
    }

    pub fn as_ptr(&self) -> *const u8 {
        self.ptr.as_ptr()
    }
}

impl std::ops::Deref for ObjectMmap {
    type Target = [u8];

    fn deref(&self) -> &[u8] {
        self.as_slice()
    }
}

impl AsRef<[u8]> for ObjectMmap {
    fn as_ref(&self) -> &[u8] {
        self.as_slice()
    }
}

impl Drop for ObjectMmap {
    fn drop(&mut self) {
        unsafe { sys_munmap(self.ptr.as_ptr() as *mut libc::c_void, self.size) };
    }
}
   

impl Drop for RecieveOnceRight {
    fn drop(&mut self) {
        let RecieveOnceRight(right, port) = *self;
        _ = unsafe { delete_receive_right(right, port)}
    }
}

impl Drop for RecieveManyRight {
    fn drop(&mut self) {
        let RecieveManyRight(right, port) = *self;
        _ = unsafe { delete_receive_right(right, port)}
    }
}

const MAP_PROT_READ: u64 = 1 << 0;
const MAP_MEM_OBJECT_IS_RIGHT: u64 = 1 << 15;

impl MemoryObjectRight {
    pub unsafe fn map(&self, offset: u64, size: u64) -> Result<ObjectMmap, Error> {
        let params = MapMemObjectParamT {
            page_table_id: 0, // Page table self
            mem_object_id_right: self.0,
            addr_start_uint: 0, // nullptr
            size_uint: size,
            offset_object: offset,
            offset_start: 0,
            object_size: size,
            access_flags: MAP_PROT_READ | MAP_MEM_OBJECT_IS_RIGHT,
        };

        let result = unsafe { map_mem_object(&params) };
        
        result.result.result().map(|()| {
            ObjectMmap {
                ptr: NonNull::new(result.value as *mut u8).expect("mapping memory object failed with null pointer"),
                size: size as usize,
            }
        })  
    }
}

impl RecieveOnceRight {
    pub fn get_id(&self) -> Right {
        self.0
    }
}

impl RecieveManyRight {
    pub fn get_id(&self) -> Right {
        self.0
    }
}

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
        unsafe { name_right_stdlib(self.0, name.as_ptr() as *const libc::c_char, name.len(), 0) }
            .result()
            .map(|_| mem::forget(self))
    }
}

pub fn get_right0() -> SendManyRight {
    SendManyRight::new(0)
}

impl Drop for MemoryObjectRight {
    fn drop(&mut self) {
        unsafe { _ = delete_right(self.0) }
    }
}

impl Drop for UnknownRight {
    fn drop(&mut self) {
        unsafe { _ = delete_right(self.0) }
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

impl From<MemoryObjectRight> for SendRight {
    fn from(right: MemoryObjectRight) -> Self {
        Self::Object(right)
    }
}

impl Drop for SendManyRight {
    fn drop(&mut self) {
        unsafe { _ = delete_right(self.0) }
    }
}

impl Drop for SendOnceRight {
    fn drop(&mut self) {
        unsafe { _ = delete_right(self.0) }
    }
}
