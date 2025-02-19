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
