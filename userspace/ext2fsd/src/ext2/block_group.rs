use endian_num::{le16, le32};
use bitflags::bitflags;

#[repr(C)]
#[derive(Debug)]
pub struct BlockGroup {
    pub bg_block_bitmap_lo: le32,
    pub bg_inode_bitmap_lo: le32,
    pub bg_inode_table_lo: le32,
    pub bg_free_blocks_count_lo: le16,
    pub bg_free_inodes_count_lo: le16,
    pub bg_used_dirs_count_lo: le16,
    pub bg_flags: le16,
    pub bg_exclude_bitmap_lo: le32,
    pub bg_block_bitmap_csum_lo: le16,
    pub bg_inode_bitmap_csum_lo: le16,
    pub bg_itable_unused_lo: le16,
    pub bg_checksum: le16,
    // The rest only exists if s_desc_size > 32 and 64bit feature is enabled
    pub bg_block_bitmap_hi: le32,
    pub bg_inode_bitmap_hi: le32,
    pub bg_inode_table_hi: le32,
    pub bg_free_blocks_count_hi: le16,
    pub bg_free_inodes_count_hi: le16,
    pub bg_used_dirs_count_hi: le16,
    pub bg_itable_unused_hi: le16,
    pub bg_exclude_bitmap_hi: le32,
    pub bg_block_bitmap_csum_hi: le16,
    pub bg_inode_bitmap_csum_hi: le16,
    pub bg_reserved: u32,
}

bitflags! {
    pub struct Flags: u32 {
        const EXT4_BG_INODE_UNINIT = 0x01;
        const EXT4_BG_BLOCK_UNINIT = 0x02;
        const EXT4_BG_INODE_ZEROED = 0x04;
    }
}