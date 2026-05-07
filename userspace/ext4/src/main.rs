#![feature(new_range_api)]

use async_trait::async_trait;
use pmos::ipc_runner::Executor;
use pmos::ipc::SendRight;
use pmos::ipc::SendManyRight;
use pmos::ipc_runner::ManyReciever;
use pmos::async_helpers::get_named_right;
use pmos::ipc_msgs::IPCMountFS;
use pmos::ipc::send_message_right;
use pmos::ipc::send_message_right_consume;
use pmos::ipc_msgs::IPC_FLAG_IO_OP_SEEK;

use futures::StreamExt;

use std::num::NonZeroU32;

use std::rc::Rc;
use std::cell::RefCell;

use ext4plus::Ext4;

use libc::ENOENT;

use ext4plus::prelude::Ext4Error;
use ext4plus::prelude::Dir;
use ext4plus::prelude::Inode;
use ext4plus::FileType;
use ext4plus::prelude::File;

#[derive(PartialEq)]
enum RunType {
    Probe,
    Mount,
    Unknown,
}

fn parse_args() -> (RunType, pmos::ipc::SendManyRight, Option<pmos::ipc::SendRight>, Option<String>) {
    let mut args = std::env::args();
    let _ = args.next(); // Skip the program name

    let mut run_type = RunType::Unknown;
    let mut disk_right: Option<pmos::ipc::SendManyRight> = None;
    let mut reply_right: Option<pmos::ipc::SendRight> = None;
    let mut mount_point: Option<String> = None;

    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--help" | "-h" => {
            println!("Usage: ext4 [--probe|--mount] --disk <disk_right> --reply <reply_right> [--mountpoint <mount_point>]");
            std::process::exit(0);
            }
            "--probe" => {
                if run_type != RunType::Unknown {
                    eprintln!("Error: Multiple run types specified");
                    std::process::exit(1);
                }
                run_type = RunType::Probe;
            }
            "--mount" => {
                if run_type != RunType::Unknown {
                    eprintln!("Error: Multiple run types specified");
                    std::process::exit(1);
                }
                run_type = RunType::Mount;
            }
            "--disk" => {
                if disk_right.is_some() {
                    eprintln!("Error: Multiple --disk arguments specified");
                    std::process::exit(1);
                }

                let right: u64 = args.next().expect("Expected disk right after --disk").parse().expect("Invalid disk right");
                let right = pmos::ipc::send_right_from_id(right).expect("Invalid disk right");
                match right {
                    pmos::ipc::SendRight::Many(r) => disk_right = Some(r),
                    _ => {
                        eprintln!("Error: Disk right must be a SendManyRight");
                        std::process::exit(1);
                    }
                }
            }
            "--reply" => {
                if reply_right.is_some() {
                    eprintln!("Error: Multiple --reply arguments specified");
                    std::process::exit(1);
                }

                let right: u64 = args.next().expect("Expected reply right after --reply").parse().expect("Invalid reply right");
                let right = pmos::ipc::send_right_from_id(right).expect("Invalid reply right");
                reply_right = Some(right);
            },
            "--mountpoint" => {
                if mount_point.is_some() {
                    eprintln!("Error: Multiple --mountpoint arguments specified");
                    std::process::exit(1);
                }
                mount_point = Some(args.next().expect("Expected mount point after --mountpoint").parse().expect("Invalid mount point"));
            }
            _ => {
                eprintln!("Error: Unknown argument {}", arg);
                std::process::exit(1);
            }
        }
    }

    if run_type == RunType::Unknown {
        eprintln!("Error: No run type specified");
        std::process::exit(1);
    }

    let disk_right = disk_right.expect("Error: --disk argument is required");

    (run_type, disk_right, reply_right, mount_point)
}

struct DiskReader {
    disk_right: Option<pmos::ipc::SendRight>,
    physical_sector_size: u32,
    logical_sector_size: u32,
    sector_count: u64,
    executor: Executor,
}

#[derive(Clone)]
struct ReaderWrapper {
    reader: Rc<RefCell<DiskReader>>,
}

impl ReaderWrapper {
    async fn new(executor: Executor, disk_right: pmos::ipc::SendManyRight) -> Self {
        let reader = DiskReader::new(executor, disk_right).await;
        Self {
            reader,
        }
    }
}

fn get_page_size() -> u64 {
    // TODO
    4096
}

#[async_trait(?Send)]
impl ext4plus::Ext4Read for ReaderWrapper {
    async fn read(
        &self,
        start_byte: u64,
        dst: &mut [u8],
    ) -> Result<(), Box<dyn std::error::Error + Send + Sync + 'static>> {
        if dst.is_empty() {
            return Ok(());
        }

        let reader = self.reader.borrow();
        let sector_size = reader.logical_sector_size as u64;
        let aligned_start_byte = (start_byte / sector_size) * sector_size;
        let byte_offset_in_first_sector = start_byte - aligned_start_byte;
        let total_bytes = byte_offset_in_first_sector
            .checked_add(dst.len() as u64)
            .ok_or_else(|| std::io::Error::new(std::io::ErrorKind::InvalidInput, "Read size overflow"))?;
        let sector_count = (total_bytes + sector_size - 1) / sector_size;
        let start_sector = aligned_start_byte / sector_size;
        let max_sector = reader.sector_count;

        if start_sector >= max_sector || start_sector + sector_count > max_sector {
            return Err(Box::new(std::io::Error::new(
                std::io::ErrorKind::UnexpectedEof,
                "Read out of bounds",
            )));
        }

        let executor = reader.executor.clone();
        drop(reader);

        let msg = pmos::ipc_msgs::IPCDiskRead::new(0, start_sector, sector_count);
        let msg = executor.send_message_reply_once(&msg, &mut self.reader.borrow_mut().disk_right, &mut [None, None, None, None]).expect("Failed to send IPCDiskRead message");
        let mut msg = msg.await
            .expect("Failed to receive IPCDiskRead reply");
        let mut maybe_right = msg.other_rights[0].take();
        let msg = msg.deserialize();

        if let pmos::ipc_msgs::Message::IPCDiskReadReply(reply) = msg {
            if reply.result != 0 {
                return Err(Box::new(std::io::Error::new(
                    std::io::ErrorKind::Other,
                    format!("Disk read failed with error code {}", -reply.result),
                )));
            }

            let right = match maybe_right.take() {
                Some(SendRight::Object(r)) => r,
                _ => {
                    return Err(Box::new(std::io::Error::new(
                        std::io::ErrorKind::Other,
                        "Expected a SendObjectRight in the reply to IPCDiskRead",
                    )))
                }
            };

            const MAX_MMAP_SIZE: u64 = 16 * 1024 * 1024; // 16 MiB
            let page_size = get_page_size();
            let mut copied = 0_u64;
            while copied < dst.len() as u64 {
                let src_offset = byte_offset_in_first_sector + copied;
                let map_offset = src_offset & (!(page_size - 1));
                let in_page_offset = src_offset - map_offset;
                let chunk_size = std::cmp::min(MAX_MMAP_SIZE, dst.len() as u64 - copied);
                let map_size = in_page_offset + chunk_size;
                let map_size_aligned = (map_size + page_size - 1) & (!(page_size - 1));
                let mmap = unsafe { right.map(map_offset, map_size_aligned).expect("Failed to map memory object right") };
                dst[copied as usize..(copied + chunk_size) as usize]
                    .copy_from_slice(&mmap[in_page_offset as usize..(in_page_offset + chunk_size) as usize]);
                copied += chunk_size;
            }

            Ok(())
        } else {
            Err(Box::new(std::io::Error::new(
                std::io::ErrorKind::Other,
                "Unexpected message type received in response to IPCDiskRead",
            )))
        }
    }
}

impl DiskReader {
    async fn new(executor: Executor, disk_right: pmos::ipc::SendManyRight) -> Rc<RefCell<Self>> {
        let mut disk_right = Some(disk_right.into());
        let msg = pmos::ipc_msgs::IPCDiskDescribe::new();
        let msg = executor.send_message_reply_once(&msg, &mut disk_right, &mut [None, None, None, None]).expect("Failed to send IPCDiskDescribe message");
        let msg = msg.await
            .expect("Failed to receive IPCDiskDescribe reply");
        let msg = msg.deserialize();

        if let pmos::ipc_msgs::Message::IPCDiskDescribeReply(reply) = msg {
            if reply.result != 0 {
                eprintln!("Error: IPCDiskDescribe failed with error code {}", -reply.result);
                std::process::exit(1);
            }

            assert!(reply.physical_sector_size > 0, "Physical sector size must be greater than 0");
            assert!(reply.physical_sector_size >= reply.logical_sector_size, "Physical sector size {} cannot be smaller than logical sector size {}", reply.physical_sector_size, reply.logical_sector_size);
            assert!(reply.physical_sector_size % reply.logical_sector_size == 0, "Physical sector size must be a multiple of logical sector size");

            Rc::new(RefCell::new(Self {
                disk_right,
                executor,
                physical_sector_size: reply.physical_sector_size,
                logical_sector_size: reply.logical_sector_size,
                sector_count: reply.sector_count,
            }))
        } else {
            eprintln!("Error: Unexpected message type received in response to IPCDiskDescribe");
            std::process::exit(1);
        }
    }
}

async fn ipc_probe(executor: Executor, reader: ReaderWrapper, reply_right: Option<SendRight>) {
    println!("ext4: Probing filesystem...");

    let fs = Ext4::load(Box::new(reader)).await;
    let msg = if let Ok(fs) = fs {
        let mut properties = Vec::new();
        let uuid = fs.uuid().as_bytes().clone();
        properties.push(pmos::ipc_msgs::FSPropertyUUID {
            uuid,
        }.into());

        let label = fs.label().to_str().unwrap_or("").to_string();
        if !label.is_empty() {
            properties.push(pmos::ipc_msgs::FSPropertyLabel {
                label,
            }.into());
        }

        properties.push(pmos::ipc_msgs::FSPropertyType {
            name: "ext4",
        }.into());

        pmos::ipc_msgs::IPCFSProbeResult {
            flags: 0,
            result: 0,
            properties,
        }
    } else {
        pmos::ipc_msgs::IPCFSProbeResult {
            flags: 0,
            result: -ENOENT as i16,
            properties: [].to_vec(),
        }
    };

    if let Some(reply_right) = reply_right {
        executor.send_message(&msg, &mut Some(reply_right), &mut [None, None, None, None]).expect("Failed to send IPCFSProbeResult message");
    }
}

async fn ipc_do_mount(executor: Executor, right: SendManyRight, mount_point: String, root_inode: u64) -> Result<(), i32> {
    println!("ext4: Mounting filesystem...");

    let mut vfsd_right = Some(get_named_right(executor.clone(), "/pmos/vfsd").await.map_err(|e| e.get())?.into());

    let msg = IPCMountFS {
        flags: 0,
        root_fd: root_inode,
        path: mount_point,
    };

    let mut rights = [Some(right.into()), None, None, None];

    let msg = executor.send_message_reply_once(&msg, &mut vfsd_right, &mut rights).map_err(|(e, _)| e.get())?.await
        .ok_or(libc::EINTR)?;

    match msg.deserialize() {
        pmos::ipc_msgs::Message::IPCMountFSReply(msg) => if msg.result == 0 {
            Ok(())
        } else {
            Err(-msg.result)
        }
        _ => {
            eprintln!("ext4: Unexpected message type received in response to IPCMountFS");
            Err(libc::EINTR)
        }
    }
}

fn ext4error_to_int(error: Ext4Error) -> i32 {
    match error {
        Ext4Error::NotAbsolute => -libc::ENOENT as i32,
        Ext4Error::NotASymlink => -libc::EINVAL as i32,
        Ext4Error::NotFound => -libc::ENOENT as i32,
        Ext4Error::IsADirectory => -libc::EISDIR as i32,
        Ext4Error::NotADirectory => -libc::ENOTDIR as i32,
        Ext4Error::IsASpecialFile => -libc::EACCES as i32,
        Ext4Error::FileTooLarge => -libc::EFBIG as i32,
        Ext4Error::NotUtf8 => -libc::EINVAL as i32,
        Ext4Error::MalformedPath => -libc::EINVAL as i32,
        Ext4Error::InvalidXattrName => -libc::EINVAL as i32,
        Ext4Error::PathTooLong => -libc::ENAMETOOLONG as i32,
        Ext4Error::TooManySymlinks => -libc::ELOOP as i32,
        Ext4Error::Encrypted => -libc::EACCES as i32,
        Ext4Error::Io(_) => -libc::EIO as i32,
        Ext4Error::UnsupportedOperation(_) => -libc::ENOSYS as i32,
        Ext4Error::Incompatible(_) => -libc::ENOSYS as i32,
        Ext4Error::Corrupt(_) => -libc::EIO as i32,
        Ext4Error::Readonly => -libc::EROFS as i32,
        Ext4Error::NoSpace => -libc::ENOSPC as i32,
        Ext4Error::AlreadyExists => -libc::EEXIST as i32,
        Ext4Error::DotEntry => -libc::EINVAL as i32,
        _ => -libc::EIO as i32,
    }
}

fn ext4direntry_error_to_int(error: ext4plus::prelude::DirEntryNameError) -> i32 {
    todo!()
}

fn ext4filetype_to_int(file_type: FileType) -> u32 {
    match file_type {
        FileType::BlockDevice => 6,
        FileType::CharacterDevice => 2,
        FileType::Directory => 4,
        FileType::Fifo => 1,
        FileType::Regular => 8,
        FileType::Socket => 12,
        FileType::Symlink => 10,
    }
}

fn ipc_resolve_path_reply(mut reply_right: Option<SendRight>, result: i32, inode: u64, file_type: u32) {
    let msg = pmos::ipc_msgs::IPCFSResolvePathReply::new(result, file_type, inode);
    let result = send_message_right(&msg, &mut reply_right, &mut [None, None, None, None]);
    if let Err(e) = result {
        eprintln!("ext4: Failed to send IPCFSResolvePathReply message: {}", e.0);
    }
}

async fn ipc_handle_resolve_path(executor: Executor, reply_right: Option<SendRight>, fs: Ext4, path_component: String, inode: u64) {
    let current_inode = u32::try_from(inode).ok().and_then(NonZeroU32::new);
    if current_inode.is_none() {
        ipc_resolve_path_reply(reply_right, -ENOENT as i32, 0, 0);
        return;
    }
    let current_inode = current_inode.unwrap();

    let entry_name = path_component.as_str().try_into();
    if let Err(e) = entry_name {
        ipc_resolve_path_reply(reply_right, ext4direntry_error_to_int(e), 0, 0);
        return;
    }
    let entry_name = entry_name.unwrap();

    let inode = Inode::read(&fs, current_inode).await;
    if let Err(e) = inode {
        ipc_resolve_path_reply(reply_right, ext4error_to_int(e), 0, 0);
        return;
    }
    let inode = inode.unwrap();

    let dir = Dir::open_inode(&fs, inode);
    if let Err(e) = dir {
        ipc_resolve_path_reply(reply_right, ext4error_to_int(e), 0, 0);
        return;
    }
    let dir = dir.unwrap();

    let entry = dir.get_entry(entry_name).await;
    if let Err(e) = entry {
        ipc_resolve_path_reply(reply_right, ext4error_to_int(e), 0, 0);
        return;
    }
    let entry = entry.unwrap();

    let file_type = entry.file_type();
    let inode = entry.index.get().into();
    ipc_resolve_path_reply(reply_right, 0, inode, ext4filetype_to_int(file_type));
}

fn ipc_fs_open_reply(reply_right: Option<SendRight>, result: i16, flags: u16, fs_right: Option<SendRight>) -> Result<(), i32> {
    let msg = pmos::ipc_msgs::IPCFSOpenReply::new(result, flags);
    let rights = [fs_right, None, None, None];
    let reply_right = reply_right.ok_or(libc::ENOENT)?;
    send_message_right_consume(&msg, reply_right, rights).map_err(|(e, _)| e.get())
}

fn ipc_read_reply(reply_right: SendRight, result: i16, flags: u16, data: &[u8]) {
    let msg = pmos::ipc_msgs::IPCReadReply {
        flags,
        result,
        data,
    };

    let result = send_message_right(&msg, &mut Some(reply_right), &mut [None, None, None, None]);
    if let Err(e) = result {
        eprintln!("ext4: Failed to send IPCReadReply message: {}", e.0);
    }
}

async fn ipc_fs_open(executor: Executor, reply_right: Option<SendRight>, fs: Ext4, _flags: u32, inode: u64) {
    let inode = u32::try_from(inode).ok().and_then(NonZeroU32::new);
    if inode.is_none() {
        _ = ipc_fs_open_reply(reply_right, -ENOENT as i16, 0, None);
        return;
    }
    let inode = inode.unwrap();

    let inode = Inode::read(&fs, inode).await;
    if let Err(e) = inode {
        _ = ipc_fs_open_reply(reply_right, ext4error_to_int(e).try_into().unwrap(), 0, None);
        return;
    }
    let inode = inode.unwrap();

    let file = File::open_inode(&fs, inode);
    if let Err(e) = file {
        _ = ipc_fs_open_reply(reply_right, ext4error_to_int(e).try_into().unwrap(), 0, None);
        return;
    }
    let mut file = file.unwrap();

    let right = executor.create_right_sendmany().expect("Failed to create SendManyRight for the filesystem");

    let result = ipc_fs_open_reply(reply_right, 0, 0, Some(right.0.into()));
    if let Err(e) = result {
        println!("Failed to reply to IPC_FS_Open request! {}", e);
        return;
    }

    let mut recieve_right = right.1;

    while let Some(mut msg) = recieve_right.next().await {
        let reply_right = msg.reply_right.take();
        match msg.deserialize() {
            pmos::ipc_msgs::Message::IPCRead(data) => {
                if reply_right.is_none() {
                    println!("ext4: Recieved read with no reply right!");
                    continue;
                }
                let reply_right = reply_right.unwrap();

                if (data.flags & IPC_FLAG_IO_OP_SEEK) != 0 {
                    let mut buffer = vec![0u8; data.max_size as usize];

                    let result = file.read_bytes(buffer.as_mut_slice()).await;
                    if let Err(e) = result {
                        let error_code = ext4error_to_int(e);
                        ipc_read_reply(reply_right, error_code as i16, 0, &[]);
                    } else {
                        let bytes_read = result.unwrap();
                        ipc_read_reply(reply_right, 0, 0, &buffer[..bytes_read as usize]);
                    }
                } else {
                    // Not implemented
                    ipc_read_reply(reply_right, -libc::ENOSYS as i16, 0, &[]);
                }
            },
            _ => {
                println!("Ext4: recieved unknown message in IPC open file consumer");
            }
        }
    }
}

async fn ipc_handle(executor: Executor, mut reciever: ManyReciever, fs: Ext4) {
    while let Some(mut msg) = reciever.next().await {
        let reply_right = msg.reply_right.take();
        match msg.deserialize() {
            pmos::ipc_msgs::Message::IPCFSResolvePath(req) => {
                executor.spawn(ipc_handle_resolve_path(executor.clone(), reply_right, fs.clone(), req.path_component, req.inode));
            }
            pmos::ipc_msgs::Message::IPCFSOpen(req) => {
                executor.spawn(ipc_fs_open(executor.clone(), reply_right, fs.clone(), req.flags, req.inode));
            }
            _ => {
                eprintln!("ext4: Received unexpected message type {}", msg.get_known_id().unwrap_or(0));
            }
        }
    }

    std::process::exit(0);
}

async fn ipc_mount(executor: Executor, reader: ReaderWrapper, mount_point: Option<String>, reply_right: Option<SendRight>) {
    let right = executor.create_right_sendmany().expect("Failed to create SendManyRight for the filesystem");
    
    let fs = Ext4::load(Box::new(reader)).await;
    if !fs.is_ok() {
        eprintln!("Error: Failed to load ext4 filesystem");
        let mut msg = pmos::ipc_msgs::IPCFSMountRequestReply::new();
        msg.result = -ENOENT as i16;

        if let Some(reply_right) = reply_right {
            executor.send_message(&msg, &mut Some(reply_right), &mut [None, None, None, None]).expect("Failed to send IPCFSMountRequestReply message");
        }
        std::process::exit(1);
    }
    let fs = fs.unwrap();

    let root_inode = fs.read_root_inode().await;
    if root_inode.is_err() {
        eprintln!("Error: Failed to read root inode of ext4 filesystem");
        let mut msg = pmos::ipc_msgs::IPCFSMountRequestReply::new();
        msg.result = -ENOENT as i16;

        if let Some(reply_right) = reply_right {
            executor.send_message(&msg, &mut Some(reply_right), &mut [None, None, None, None]).expect("Failed to send IPCFSMountRequestReply message");
        }
        std::process::exit(1);
    }

    executor.spawn(ipc_handle(executor.clone(), right.1, fs));
    
    let mount = ipc_do_mount(executor.clone(), right.0, mount_point.unwrap_or("/".to_string()), root_inode.unwrap().index.get().into()).await;

    let mut msg = pmos::ipc_msgs::IPCFSMountRequestReply::new();
    msg.result = if let Err(e) = mount {
        eprintln!("Error: Failed to mount ext4 filesystem: {}", e);
        -e as i16
    } else {
        0
    };

    if let Some(reply_right) = reply_right {
        executor.send_message(&msg, &mut Some(reply_right), &mut [None, None, None, None]).expect("Failed to send IPCFSMountRequestReply message");
    }

    if mount.is_err() {
        std::process::exit(1);
    }
}    

async fn handle_ipc(executor: Executor, run_type: RunType, disk_right: pmos::ipc::SendManyRight, reply_right: Option<pmos::ipc::SendRight>, mount_point: Option<String>) {
    let reader = ReaderWrapper::new(executor.clone(), disk_right).await;

    match run_type {
        RunType::Probe => {
            ipc_probe(executor, reader, reply_right).await;
            std::process::exit(0);
        }
        RunType::Mount => {
            ipc_mount(executor, reader, mount_point, reply_right).await;
        }
        _ => unreachable!(),
    }
}

fn main() {
    let (run_type, disk_right, reply_right, mount_point) = parse_args();

    let executor = Executor::new();
    executor.spawn(handle_ipc(executor.clone(), run_type, disk_right, reply_right, mount_point));
    executor.run();
}
