use bitflags::bitflags;

#[repr(C)]
#[derive(Debug)]
pub struct Ext2Superblock {
    pub inodes: u32,
    pub blocks: u32,
    pub superuser_blocks: u32,
    pub unallocated_blocks: u32,
    pub unallocated_inodes: u32,
    pub superblock_block: u32,
    pub block_size: u32,
    pub fragment_size: u32,
    pub blocks_per_group: u32,
    pub fragments_per_group: u32,
    pub inodes_per_group: u32,
    pub last_mount_time: u32,
    pub last_written_time: u32,
    pub mounts_before_chk: u16,
    pub max_moununts_before_chk: u16,
    pub signature: u16,
    pub fs_state: u16,
    pub error_action: u16,
    pub verion_minor: u16,
    pub last_chk_time: u32,
    pub forced_chk_interval: u32,
    pub os_creator_id: u32,
    pub version_major: u32,
    pub reserved_user_id: u16,
    pub reserved_group_id: u16,
}

#[repr(C)]
#[derive(Debug)]
pub struct Ext2SuperblockExtended {
    pub base: Ext2Superblock,
    pub first_avail_inode: u32,
    pub inode_size_bytes: u16,
    pub superblock_block_group: u16,
    pub opt_features: u32,
    pub required_features: u32,
    pub rw_required_features: u32,
    pub fs_id: [u8; 16],
    pub volume_name: [u8; 16],
    pub path_last_mounted: [u8; 64],
    pub compression: u32,
    pub prealloc_blocks_files: u8,
    pub prealloc_blocks_dirs: u8,
    pub unused: u16,
    pub journal_id: [u8; 16],
    pub journal_inode: u32,
    pub journal_device: u32,
    pub orphal_list_head: u32,
}

bitflags! {
    pub struct OptionalFeatures: u32 {
        const PREALLOC_DIRS = 0x01;
        const AFS_SERVER_INODES = 0x02;
        const JOURNALED = 0x04;
        const EXTENDED_ATTRS = 0x08;
        const SELF_RESIZE = 0x10;
        const DIRS_HASH_IDX = 0x20;
    }

    pub struct RequiredFeatures: u32 {
        const COMPRESSION = 0x01;
        const DIRENTS_TYPE = 0x02;
        const JOURNAL_REPLAY = 0x04;
        const JOURNAL_DEVICE = 0x08;
    }

    pub struct ReadOnlyFeatures: u32 {
        const SPARSE_SUPERBLOCKS = 0x01;
        const FILE_SIZE_64BIT = 0x02;
        const DIRECTORIES_BIN_TREE = 0x04;
    }
}