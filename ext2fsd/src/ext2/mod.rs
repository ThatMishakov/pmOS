pub mod superblock;
pub mod block_group;

#[repr(C)]
#[derive(Debug)]
pub struct Inode {
    pub type_permissions: u16,
    pub user_id: u16,
    pub size_lower: u32,
    pub atime: u32,
    pub ctime: u32,
    pub mtime: u32,
    pub dtime: u32,
    pub group_id: u16,
    pub dirents: u16,
    pub disk_sectors: u32,
    pub flags: u32,
    pub os_specific_val1: u32,
    pub direct_ptr: [u32; 12],
    pub indirect_ptr: u32,
    pub double_indirect: u32,
    pub triply_indirect: u32,
    pub generation: u32,
    pub extended_attrs_block: u32,
    pub upper_size_dir_acl: u32,
    pub fragment_addr: u32,
    pub os_specific2: [u32; 3],
}

impl Inode {
    pub fn inode_type(i: &Self) -> u8 {
        (i.type_permissions >> 12) as u8
    }
}

pub const FIFO: u8 = 0x01;
pub const CHAR_DEVICE: u8 = 0x02;
pub const DIRECTORY: u8 = 0x04;
pub const BLOCK_DEVICE: u8 = 0x06;
pub const FILE: u8 = 0x08;
pub const SYMLINK: u8 = 0x0A;
pub const SOCKET: u8 = 0x0C;