use endian_num::{le16, le32};

#[repr(C)]
#[derive(Debug)]
pub struct Inode {
    pub type_permissions: le16,
    pub user_id: le16,
    pub size_lower: le32,
    pub atime: le32,
    pub ctime: le32,
    pub mtime: le32,
    pub dtime: le32,
    pub group_id: le16,
    pub dirents: le16,
    pub disk_sectors: le32,
    pub flags: le32,
    pub os_specific_val1: le32,
    pub direct_ptr: [le32; 12],
    pub indirect_ptr: le32,
    pub double_indirect: le32,
    pub triply_indirect: le32,
    pub generation: le32,
    pub extended_attrs_block: le32,
    pub upper_size_dir_acl: le32,
    pub fragment_addr: le32,
    pub os_specific2: [le32; 3],
}

impl Inode {
    pub fn inode_type(i: &Self) -> u8 {
        (i.type_permissions.to_ne() >> 12) as u8
    }
}

pub const FIFO: u8 = 0x01;
pub const CHAR_DEVICE: u8 = 0x02;
pub const DIRECTORY: u8 = 0x04;
pub const BLOCK_DEVICE: u8 = 0x06;
pub const FILE: u8 = 0x08;
pub const SYMLINK: u8 = 0x0A;
pub const SOCKET: u8 = 0x0C;