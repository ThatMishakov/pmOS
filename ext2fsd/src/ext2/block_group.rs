#[repr(C)]
#[derive(Debug)]
pub struct BlockGroup {
    pub block_usage_bitmap: u32,
    pub inode_usage_bitmap: u32,
    pub inode_table: u32,
    pub unallocated_blocks: u32,
    pub unallocated_inodes: u32,
    pub directories_count: u32,
}
