#![feature(new_range_api)]

use async_trait::async_trait;
use pmos::ipc_runner::Executor;
use pmos::ipc::SendRight;

use std::rc::Rc;
use std::cell::RefCell;

use ext4plus::Ext4;

#[derive(PartialEq)]
enum RunType {
    Probe,
    Mount,
    Unknown,
}

fn parse_args() -> (RunType, pmos::ipc::SendManyRight, Option<pmos::ipc::SendRight>) {
    let mut args = std::env::args();
    let _ = args.next(); // Skip the program name

    let mut run_type = RunType::Unknown;
    let mut disk_right: Option<pmos::ipc::SendManyRight> = None;
    let mut reply_right: Option<pmos::ipc::SendRight> = None;

    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--help" | "-h" => {
            println!("Usage: ext4 [--probe|--mount] --disk <disk_right> --reply <reply_right>");
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

    (run_type, disk_right, reply_right)
}

struct DiskReader {
    disk_right: Option<pmos::ipc::SendRight>,
    physical_sector_size: u32,
    logical_sector_size: u32,
    sector_count: u64,
    executor: Executor,
}

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
        let sector_size = self.reader.borrow().logical_sector_size as u64;
        let sector_count = (dst.len() as u64 + sector_size - 1) / sector_size;

        let physical_sector_size = self.reader.borrow().physical_sector_size as u64;
        let physical_start_sector = start_byte / physical_sector_size;
        let physical_sector_count = sector_count * (sector_size / physical_sector_size);
        let max_sector = self.reader.borrow().sector_count;
        if physical_start_sector >= max_sector || physical_start_sector + physical_sector_count > max_sector {
            return Err(Box::new(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "Read out of bounds")));
        }

        let executor = self.reader.borrow().executor.clone();

        let msg = pmos::ipc_msgs::IPCDiskRead::new(0, physical_start_sector, physical_sector_count);
        let msg = executor.send_message_reply_once(&msg, &mut self.reader.borrow_mut().disk_right, &mut [None, None, None, None]).expect("Failed to send IPCDiskRead message");
        let mut msg = msg.await
            .expect("Failed to receive IPCDiskRead reply");
        let mut maybe_right = msg.other_rights[0].take();
        let msg = msg.deserialize();

        if let pmos::ipc_msgs::Message::IPCDiskReadReply(reply) = msg {
            if reply.result != 0 {
                return Err(Box::new(std::io::Error::new(std::io::ErrorKind::Other, format!("Disk read failed with error code {}", -reply.result))));
            }

            let right = match maybe_right.take() {
                Some(SendRight::Object(r)) => r,
                _ => return Err(Box::new(std::io::Error::new(std::io::ErrorKind::Other, "Expected a SendObjectRight in the reply to IPCDiskRead"))),
            };

            const MAX_MMAP_SIZE: u64 = 16 * 1024 * 1024; // 16 MiB
            let mut offset = 0;
            while offset < dst.len() as u64 {
                let chunk_size = std::cmp::min(MAX_MMAP_SIZE, dst.len() as u64 - offset);
                // Align chunk_size up to page size
                let page_size = get_page_size();
                let chunk_size_aligned = (chunk_size + page_size - 1) & (!(page_size - 1));
                let mmap = unsafe { right.map(offset, chunk_size_aligned).expect("Failed to map memory object right") };
                dst[offset as usize..(offset + chunk_size) as usize].copy_from_slice(&mmap[..chunk_size as usize]);
                offset += chunk_size;
            }

            Ok(())
        } else {
            Err(Box::new(std::io::Error::new(std::io::ErrorKind::Other, "Unexpected message type received in response to IPCDiskRead")))
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
            assert!(reply.physical_sector_size <= reply.logical_sector_size, "Physical sector size cannot be larger than logical sector size");
            assert!(reply.logical_sector_size % reply.physical_sector_size == 0, "Logical sector size must be a multiple of physical sector size");

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
    let fs = Ext4::load(Box::new(reader)).await;
    println!("{:#?}", fs);
}
    

async fn handle_ipc(executor: Executor, run_type: RunType, disk_right: pmos::ipc::SendManyRight, reply_right: Option<pmos::ipc::SendRight>) {
    let reader = ReaderWrapper::new(executor.clone(), disk_right).await;

    match run_type {
        RunType::Probe => {
            ipc_probe(executor, reader, reply_right).await;
        }
        RunType::Mount => {
            todo!();
        }
        _ => unreachable!(),
    }

    std::process::exit(0);
}

fn main() {
    println!("Hello from ext4!");

    let (run_type, disk_right, reply_right) = parse_args();

    let executor = Executor::new();
    executor.spawn(handle_ipc(executor.clone(), run_type, disk_right, reply_right));
    executor.run();
}
