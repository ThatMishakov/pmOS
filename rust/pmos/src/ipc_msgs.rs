use bytemuck::{Pod, Zeroable};
use std::borrow::Cow;

use crate::pmbus::{ObjectProperties, pmbus_object_serialize};

#[repr(C)]
pub struct IPCGenericMsg {
    pub msg_type: u32,
}

pub const IPC_BUS_PUBLISH_OBJECT_NUM: u32 = 0x1a0;
#[derive(Debug)]
pub struct IPCBusPublishObject<'a> {
    pub flags: u32,
    pub reply_port: super::ipc::Port,
    pub user_arg: u64,
    pub object_data: &'a [u8],
}

pub const IPC_BUS_REQUEST_OBJECT_NUM: u32 = 0x1a1;
#[derive(Debug)]
pub struct IPCBusRequestObject<'a> {
    pub flags: u32,
    pub start_sequence_number: u64,
    pub filter_data: &'a [u8],
}

pub const IPC_BUS_PUBLISH_OBJECT_REPLY_NUM: u32 = 0x1b0;
#[repr(C)]
#[derive(Debug, Copy, Clone, Zeroable, Pod)]
pub struct IPCBusPublishObjectReply {
    msg_type: u32,
    flags: u32,
    pub result: i32,
    reserved: u32,
    pub user_arg: u64,
    pub sequence_number: u64,
}

impl IPCBusPublishObjectReply {
    pub fn new() -> IPCBusPublishObjectReply {
        IPCBusPublishObjectReply {
            msg_type: IPC_BUS_PUBLISH_OBJECT_REPLY_NUM,
            flags: 0,
            result: 0,
            reserved: 0,
            user_arg: 0,
            sequence_number: 0,
        }
    }
}


pub const IPC_BUS_REQUEST_OBJECT_REPLY_NUM: u32 = 0x1b1;
#[derive(Debug)]
#[repr(C)]
pub struct IPCBusRequestObjectReply<'a> {
    pub flags: u32,
    pub result: i32,
    pub next_sequence_number: u64,
    pub object: Option<(&'a str, &'a ObjectProperties)>,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Zeroable, Pod)]
struct IPCBusRequestObjectReplyHdr {
    msg_type: u32,
    flags: u32,
    result: i32,
    reserved: u32,
    next_sequence_number: u64,
}

fn push_pod<T: Pod>(out: &mut Vec<u8>, v: &T) {
    out.extend_from_slice(bytemuck::bytes_of(v));
}

impl Serializable for IPCBusRequestObjectReply<'_> {
    fn serialize(&self) -> Cow<[u8]> {
        let hdr = IPCBusRequestObjectReplyHdr {
            msg_type: IPC_BUS_REQUEST_OBJECT_REPLY_NUM,
            flags: self.flags,
            result: self.result, 
            reserved: 0,
            next_sequence_number: self.next_sequence_number,
        };

        let object_data = self.object.map_or(vec![], |o| pmbus_object_serialize(o.0, o.1));
        let mut out = Vec::with_capacity(core::mem::size_of::<IPCBusRequestObjectReplyHdr>() + object_data.len());
        push_pod(& mut out, &hdr);
        out.extend_from_slice(&object_data);
        Cow::<[u8]>::from(out)
    }
}

impl IPCBusRequestObjectReply<'_> {
    pub fn new() -> Self {
        IPCBusRequestObjectReply{
            flags: 0,
            result: 0,
            next_sequence_number: 0,
            object: None,
        }
    }
}

pub enum Message<'a> {
    IPCBusPublishObject(IPCBusPublishObject<'a>),
    IPCBusRequestObject(IPCBusRequestObject<'a>),
    Unknown,
}

impl super::ipc::Message {
    pub fn deserialize(&self) -> Message {
        if let Some(i) = self.get_known_id() {
            let data = &self.data;
            match i {
                IPC_BUS_PUBLISH_OBJECT_NUM => {
                    if data.len() < 24 {
                        return Message::Unknown;
                    }

                    Message::IPCBusPublishObject(IPCBusPublishObject {
                        flags: u32::from_ne_bytes(data[4..8].try_into().unwrap()),
                        reply_port: u64::from_ne_bytes(data[8..16].try_into().unwrap()),
                        user_arg: u64::from_ne_bytes(data[16..24].try_into().unwrap()),
                        object_data: &data[24..],
                    })
                }
                IPC_BUS_REQUEST_OBJECT_NUM => {
                    if data.len() < 16 {
                        return Message::Unknown;
                    }

                    Message::IPCBusRequestObject(IPCBusRequestObject {
                        flags: u32::from_ne_bytes(data[4..8].try_into().unwrap()),
                        start_sequence_number: u64::from_ne_bytes(data[8..16].try_into().unwrap()),
                        filter_data: &data[16..],
                    })
                }
                _ => Message::Unknown,
            }
        } else {
            Message::Unknown
        }
    }
}

pub trait Serializable {
    fn serialize(&self) -> Cow<[u8]>;
}

impl<T: Pod> Serializable for T {
    fn serialize(&self) -> Cow<[u8]> {
        Cow::Borrowed(bytemuck::bytes_of(self))
    }
}
