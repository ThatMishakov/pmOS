use bitflags::bitflags;
use endian_num::{le16, le32, le64};

#[repr(C)]
#[derive(Debug)]
pub struct Ext2Superblock {
    pub inodes: le32,
    pub blocks: le32,
    pub superuser_blocks: le32,
    pub unallocated_blocks: le32,
    pub unallocated_inodes: le32,
    pub superblock_block: le32,
    pub block_size: le32,
    pub fragment_size: le32,
    pub blocks_per_group: le32,
    pub fragments_per_group: le32,
    pub inodes_per_group: le32,
    pub last_mount_time: le32,
    pub last_written_time: le32,
    pub mounts_before_chk: le16,
    pub max_moununts_before_chk: le16,
    pub signature: le16,
    pub fs_state: le16,
    pub error_action: le16,
    pub verion_minor: le16,
    pub last_chk_time: le32,
    pub forced_chk_interval: le32,
    pub os_creator_id: le32,
    pub version_major: le32,
    pub reserved_user_id: le16,
    pub reserved_group_id: le16,
}

#[repr(C)]
#[derive(Debug)]
pub struct Ext2SuperblockExtended {
    pub base: Ext2Superblock,
    pub s_first_ino: le32,
    pub s_inode_size: le16,
    pub s_block_group_nr: le16,
    pub s_feature_compat: le32,
    pub s_feature_incompat: le32,
    pub s_feature_ro_compat: le32,
    pub s_uuid: [u8; 16],
    pub s_volume_name: [u8; 16],
    pub s_last_mounted: [u8; 64],
    pub s_algorithm_usage_bitmap: le32,
    pub s_prealloc_blocks: u8,
    pub s_prealloc_dir_blocks: u8,
    pub s_reserved_gdt_blocks: le16,
    pub s_journal_uuid: [u8; 16],
    pub s_journal_inum: le32,
    pub s_journal_dev: le32,
    pub journal_device: le32,
    pub s_last_orphan: le32,
    pub s_hash_seed: [le32; 4],
    pub s_def_hash_version: u8,
    pub s_jnl_backup_type: u8,
    pub s_desc_size: le16,
    pub s_default_mount_opts: le32,
    pub s_first_meta_bg: le32,
    pub s_mkfs_time: le32,
    pub s_jnl_blocks: [le32; 17],
    pub s_blocks_count_hi: le32,
    pub s_r_blocks_count_hi: le32,
    pub s_free_blocks_count_hi: le32,
    pub s_min_extra_isize: le16,
    pub s_want_extra_isize: le16,
    pub s_flags: le16,
    pub s_raid_stride: le16,
    pub s_mmp_interval: le16,
    pub s_mmp_block: le64,
    pub s_raid_stripe_width: le32,
    pub s_log_groups_per_flex: u8,
    pub s_checksum_type: u8,
    pub s_reserved_pad: le16,
    pub s_kbytes_written: le64,
    pub s_snapshot_inum: le32,
    pub s_snapshot_id: le32,
    pub s_snapshot_r_blocks_count: le64,
    pub s_snapshot_list: le32,
    pub s_error_count: le32,
    pub s_first_error_time: le32,
    pub s_first_error_ino: le32,
    pub s_first_error_block: le64,
    pub s_first_error_func: [u8; 32],
    pub s_first_error_line: le32,
    pub s_last_error_time: le32,
    pub s_last_error_ino: le32,
    pub s_last_error_line: le32,
    pub s_last_error_block: le64,
    pub s_last_error_func: [u8; 32],
    pub s_mount_opts: [u8; 64],
    pub s_usr_quota_inum: le32,
    pub s_grp_quota_inum: le32,
    pub s_overhead_blocks: le32,
    pub s_backup_bgs: [le32; 2],
    pub s_encrypt_algos: [u8; 4],
    pub s_encrypt_pw_salt: [u8; 16],
    pub s_lpf_ino: le32,
    pub s_prj_quota_inum: le32,
    pub s_checksum_seed: le32,
    pub s_wtime_hi: u8,
    pub s_mtime_hi: u8,
    pub s_mkfs_time_hi: u8,
    pub s_lastcheck_hi: u8,
    pub s_first_error_time_hi: u8,
    pub s_last_error_time_hi: u8,
    pub s_pad: [u8; 2],
    pub s_encoding: le16,
    pub s_encoding_flags: le16,
    pub s_orphan_file_inum: le32,
    pub s_reserved: [le32; 94],
    pub s_checksum: le32,
}

bitflags! {
    pub struct OptionalFeatures: u32 {
        const COMPAT_DIR_PREALLOC = 0x01;
        const COMPAT_IMAGIC_INODES = 0x02;
        const COMPAT_HAS_JOURNAL = 0x04;
        const COMPAT_EXT_ATTR = 0x08;
        const COMPAT_RESIZE_INODE = 0x10;
        const COMPAT_DIR_INDEX = 0x20;
        const COMPAT_LAZY_BG = 0x40;
        const COMPAT_EXCLUDE_INODE = 0x80;
        const COMPAT_EXCLUDE_BITMAP = 0x100;
        const COMPAT_SPARSE_SUPER2 = 0x200;
        const COMPAT_FAST_COMMIT = 0x400;
        const COMPAT_ORPHAN_PRESENT = 0x1000;
    }

    pub struct RequiredFeatures: u32 {
        const INCOMPAT_COMPRESSION = 0x01;
        const INCOMPAT_FILETYPE = 0x02;
        const INCOMPAT_RECOVER = 0x04;
        const INCOMPAT_JOURNAL_DEV = 0x08;
        const INCOMPAT_META_BG = 0x10;
        const INCOMPAT_EXTENTS = 0x40;
        const INCOMPAT_64BIT = 0x80;
        const INCOMPAT_MMP = 0x100;
        const INCOMPAT_FLEX_BG = 0x200;
        const INCOMPAT_EA_INODE = 0x400;
        const INCOMPAT_DIRDATA = 0x1000;
        const INCOMPAT_CSUM_SEED= 0x2000;
        const INCOMPAT_LARGEDIR = 0x4000;
        const INCOMPAT_INLINE_DATA = 0x8000;
        const INCOMPAT_ENCRYPT = 0x10000;
    }

    pub struct ReadOnlyFeatures: u32 {
        const RO_COMPAT_SPARSE_SUPER = 0x01;
        const RO_COMPAT_LARGE_FILE = 0x02;
        const RO_COMPAT_BTREE_DIR = 0x04;
        const RO_COMPAT_HUGE_FILE = 0x08;
        const RO_COMPAT_GDT_CSUM = 0x10;
        const RO_COMPAT_DIR_NLINK = 0x20;
        const RO_COMPAT_EXTRA_ISIZE = 0x40;
        const RO_COMPAT_HAS_SNAPSHOT = 0x80;
        const RO_COMPAT_QUOTA = 0x100;
        const RO_COMPAT_BIGALLOC = 0x200;
        const RO_COMPAT_METADATA_CSUM = 0x400;
        const RO_COMPAT_REPLICA = 0x800;
        const RO_COMPAT_READONLY = 0x1000;
        const RO_COMPAT_PROJECT = 0x2000;
        const RO_COMPAT_VERITY = 0x8000;
        const RO_COMPAT_ORPHAN_PRESENT = 0x10000;
    }
}
