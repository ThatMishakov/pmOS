use bytemuck::{Pod, Zeroable};
use std::borrow::Cow;
use bytemuck::checked::try_from_bytes;

use crate::pmbus::{ObjectProperties, pmbus_object_serialize};

#[repr(C)]
pub struct IPCGenericMsg {
    pub msg_type: u32,
}

pub const IPC_BUS_PUBLISH_OBJECT_NUM: u32 = 0x1a0;
#[derive(Debug)]
pub struct IPCBusPublishObject<'a> {
    pub flags: u32,
    pub object_data: &'a [u8],
}

pub const IPC_BUS_REQUEST_OBJECT_NUM: u32 = 0x1a1;
#[derive(Debug)]
pub struct IPCBusRequestObject<'a> {
    pub flags: u32,
    pub start_sequence_number: u64,
    pub filter_data: &'a [u8],
}

pub const IPC_NAME_RIGHT_NUM: u32 = 0x1c0;
#[derive(Debug)]
pub struct IPCNameRight<'a> {
    pub flags: u32,
    pub name: &'a str,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Zeroable, Pod)]
struct IPCNameRightHdr {
    msg_type: u32,
    flags: u32,
}

pub const IPC_NAME_RIGHT_REPLY_NUM: u32 = 0x1d0;
#[repr(C)]
#[derive(Debug, Copy, Clone, Zeroable, Pod)]
pub struct IPCNameRightReply {
    msg_type: u32,
    pub flags: u32,
    pub result: i32,
}

pub const IPC_BUS_PUBLISH_OBJECT_REPLY_NUM: u32 = 0x1b0;
#[repr(C)]
#[derive(Debug, Copy, Clone, Zeroable, Pod)]
pub struct IPCBusPublishObjectReply {
    msg_type: u32,
    flags: u32,
    pub result: i32,
    reserved: u32,
    pub sequence_number: u64,
}

impl IPCBusPublishObjectReply {
    pub fn new() -> IPCBusPublishObjectReply {
        IPCBusPublishObjectReply {
            msg_type: IPC_BUS_PUBLISH_OBJECT_REPLY_NUM,
            flags: 0,
            result: 0,
            reserved: 0,
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
    pub object_id: u64,
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
    object_id: u64,
}

pub const IPC_DISK_READ_NUM: u32 = 0xF2;
#[repr(C)]
#[derive(Copy, Clone, Debug, Zeroable, Pod)]
pub struct IPCDiskRead {
    msg_type: u32,
    pub flags: u32,
    pub start_sector: u64,
    pub sector_count: u64,
}

impl IPCDiskRead {
    pub fn new(flags: u32, start_sector: u64, sector_count: u64) -> Self {
        IPCDiskRead {
            msg_type: IPC_DISK_READ_NUM,
            flags,
            start_sector,
            sector_count,
        }
     }
}

pub const IPC_DISK_DESCRIBE_NUM: u32 = 0xF6;
#[repr(C)]
#[derive(Copy, Clone, Debug, Zeroable, Pod)]
pub struct IPCDiskDescribe {
    msg_type: u32,
    flags: u32,
}

impl IPCDiskDescribe {
    pub fn new() -> Self {
        IPCDiskDescribe {
            msg_type: IPC_DISK_DESCRIBE_NUM,
            flags: 0,
        }
    }
}

pub const IPC_DISK_READ_REPLY_NUM: u32 = 0xFA;
#[repr(C)]
#[derive(Debug, Copy, Clone, Zeroable, Pod)]
pub struct IPCDiskReadReply {
    msg_type: u32,
    pub flags: u16,
    pub result: i16,
}

pub const IPC_DISK_DESCRIBE_REPLY_NUM: u32 = 0xFE;
#[repr(C)]
#[derive(Copy, Clone, Debug, Zeroable, Pod)]
pub struct IPCDiskDescribeReply {
    msg_type: u32,
    pub flags: u16,
    pub result: i16,
    pub sector_count: u64,
    pub logical_sector_size: u32,
    pub physical_sector_size: u32,
}

#[derive(Clone, Debug)]
pub struct FSPropertyType {
    pub name: &'static str,
}
#[derive(Clone, Debug)]
pub struct FSPropertyUUID {
    pub uuid: [u8; 16],
}
#[derive(Clone, Debug)]
pub struct FSPropertyLabel {
    pub label: String,
}

const PROPERTY_TYPE_TYPE: u32 = 1;
const PROPERTY_TYPE_UUID: u32 = 2;
const PROPERTY_TYPE_LABEL: u32 = 3;
#[derive(Clone, Debug)]
pub enum FSProperty {
    Type(FSPropertyType),
    UUID(FSPropertyUUID),
    Label(FSPropertyLabel),
    Unknown(u32, u32, Vec<u8>),
}

impl From<FSPropertyType> for FSProperty {
    fn from(value: FSPropertyType) -> Self {
        FSProperty::Type(value)
    }
}
impl From<FSPropertyUUID> for FSProperty {
    fn from(value: FSPropertyUUID) -> Self {
        FSProperty::UUID(value)
    }
}
impl From<FSPropertyLabel> for FSProperty {
    fn from(value: FSPropertyLabel) -> Self {
        FSProperty::Label(value)
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Zeroable, Pod)]
struct IPCFSPropertyHdr {
    property_type: u32,
    flags: u32,
    // Total length of the property, including this header, aligned to 8 bytes
    length: u32,
    data_length: u32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Zeroable, Pod)]
struct IPCFSProbeResultHdr {
    msg_type: u32,
    flags: u16,
    result: i16,
}

pub const IPC_FS_PROBE_RESULT_NUM: u32 = 0xD7;
#[repr(C)]
pub struct IPCFSProbeResult {
    pub flags: u16,
    pub result: i16,
    pub properties: Vec<FSProperty>,
}

fn push_pod<T: Pod>(out: &mut Vec<u8>, v: &T) {
    out.extend_from_slice(bytemuck::bytes_of(v));
}

impl Serializable for IPCFSProbeResult {
    fn serialize(&self) -> Cow<'_, [u8]> {
        let hdr = IPCFSProbeResultHdr {
            msg_type: IPC_FS_PROBE_RESULT_NUM,
            flags: self.flags,
            result: self.result,
        };

        let mut out = Vec::with_capacity(core::mem::size_of::<IPCFSProbeResultHdr>() + self.properties.iter().map(|p| {
            let data_len = core::mem::size_of::<IPCFSPropertyHdr>() + match p {
                FSProperty::Type(t) => t.name.len(),
                FSProperty::UUID(_) => 16,
                FSProperty::Label(l) => l.label.len(),
                FSProperty::Unknown(_, _, d) => d.len(),
            };
            // Align to 8 bytes
            (data_len + 7) & !7
        }).sum::<usize>());
        push_pod(&mut out, &hdr);

        for prop in &self.properties {
            let (property_type, flags, data): (u32, u32, Vec<u8>) = match prop {
                FSProperty::Type(t) => (PROPERTY_TYPE_TYPE, 0, t.name.as_bytes().to_vec()),
                FSProperty::UUID(u) => (PROPERTY_TYPE_UUID, 0, u.uuid.to_vec()),
                FSProperty::Label(l) => (PROPERTY_TYPE_LABEL, 0, l.label.as_bytes().to_vec()),
                FSProperty::Unknown(t, f, d) => (*t, *f, d.clone()),
            };

            let length_aligned = (data.len() + 7) & !7;
            let prop_hdr = IPCFSPropertyHdr {
                property_type,
                flags,
                length: (core::mem::size_of::<IPCFSPropertyHdr>() + length_aligned) as u32,
                data_length: data.len() as u32,
            };
            push_pod(&mut out, &prop_hdr);
            out.extend_from_slice(&data);
            // Pad to 8 bytes
            out.extend_from_slice(&vec![0u8; length_aligned - data.len()]);
        }
        Cow::from(out)
    }
}
                

impl Serializable for IPCBusRequestObjectReply<'_> {
    fn serialize(&self) -> Cow<'_, [u8]> {
        let hdr = IPCBusRequestObjectReplyHdr {
            msg_type: IPC_BUS_REQUEST_OBJECT_REPLY_NUM,
            flags: self.flags,
            result: self.result, 
            reserved: 0,
            next_sequence_number: self.next_sequence_number,
            object_id: self.object_id,
        };

        let object_data = self.object.map_or(vec![], |o| pmbus_object_serialize(o.0, o.1));
        let mut out = Vec::with_capacity(core::mem::size_of::<IPCBusRequestObjectReplyHdr>() + object_data.len());
        push_pod(& mut out, &hdr);
        out.extend_from_slice(&object_data);
        Cow::<[u8]>::from(out)
    }
}

impl Serializable for IPCNameRight<'_> {
    fn serialize(&self) -> Cow<'_, [u8]> {
        let hdr = IPCNameRightHdr {
            msg_type: IPC_NAME_RIGHT_NUM,
            flags: 0,
        };

        let mut out = Vec::with_capacity(core::mem::size_of::<IPCNameRightHdr>() + self.name.len());
        push_pod(&mut out, &hdr);
        out.extend_from_slice(self.name.as_bytes());
        Cow::<[u8]>::from(out)
    }
}

impl IPCBusRequestObjectReply<'_> {
    pub fn new() -> Self {
        IPCBusRequestObjectReply{
            flags: 0,
            result: 0,
            next_sequence_number: 0,
            object_id: 0,
            object: None,
        }
    }
}

#[derive(Debug)]
pub enum Message<'a> {
    IPCBusPublishObject(IPCBusPublishObject<'a>),
    IPCBusRequestObject(IPCBusRequestObject<'a>),
    IPCNameRightReply(IPCNameRightReply),
    IPCDiskDescribeReply(IPCDiskDescribeReply),
    IPCDiskReadReply(IPCDiskReadReply),
    Unknown,
}

impl super::ipc::Message {
    pub fn deserialize(&self) -> Message<'_> {
        if let Some(i) = self.get_known_id() {
            let data = &self.data;
            match i {
                IPC_BUS_PUBLISH_OBJECT_NUM => {
                    if data.len() < 8 {
                        return Message::Unknown;
                    }

                    Message::IPCBusPublishObject(IPCBusPublishObject {
                        flags: u32::from_ne_bytes(data[4..8].try_into().unwrap()),
                        object_data: &data[8..],
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
                IPC_NAME_RIGHT_REPLY_NUM =>
                    try_from_bytes::<IPCNameRightReply>(data).map(|data| Message::IPCNameRightReply(data.clone())).unwrap_or(Message::Unknown),
                IPC_DISK_DESCRIBE_REPLY_NUM =>
                    try_from_bytes::<IPCDiskDescribeReply>(data).map(|data| Message::IPCDiskDescribeReply(data.clone())).unwrap_or(Message::Unknown),
                IPC_DISK_READ_REPLY_NUM =>
                    try_from_bytes::<IPCDiskReadReply>(data).map(|data| Message::IPCDiskReadReply(data.clone())).unwrap_or(Message::Unknown),
                _ => Message::Unknown,
            }
        } else {
            Message::Unknown
        }
    }
}

pub trait Serializable {
    fn serialize(&self) -> Cow<'_, [u8]>;
}

impl<T: Pod> Serializable for T {
    fn serialize(&self) -> Cow<'_, [u8]> {
        Cow::Borrowed(bytemuck::bytes_of(self))
    }
}
