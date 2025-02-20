use bytemuck::{Pod, Zeroable};
use std::borrow::Cow;

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

pub enum Message<'a> {
    IPCBusPublishObject(IPCBusPublishObject<'a>),
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
