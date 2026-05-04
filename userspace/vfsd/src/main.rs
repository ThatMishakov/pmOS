#![feature(new_range_api)]

use pmos::ipc_runner::Executor;
use pmos::async_helpers;
use futures::StreamExt;
use std::rc::Rc;
use std::cell::RefCell;
use std::process;
use pmos::ipc::SendRight;
use std::collections::BTreeMap;

struct Filesystem {
    right: Option<SendRight>,
    mountpoint: String,
}

struct VNode {
    parent_fs: Rc<RefCell<Filesystem>>,
    parent: Option<u64>,
    inode: u64,
    name: String,
    is_dir: bool,
}

struct VFS {
    filesystems: Vec<Rc<RefCell<Filesystem>>>,
    vnodes: BTreeMap<u64, Rc<RefCell<VNode>>>,

    root_vnode: Option<Rc<RefCell<VNode>>>,
}

fn mount_filesystem_reply(mut reply_right: Option<SendRight>, result: i32) {
    let msg = pmos::ipc_msgs::IPCMountFSReply::new(result);

    if reply_right.is_some() {
        let result = pmos::ipc::send_message_right(&msg, &mut reply_right, &mut [None, None, None, None]);
        if let Err(e) = result {
            println!("Failed to send IPCMountFSReply: {:?}", e);
        }
    }
}

fn mount_filesystem(vfs: Rc<RefCell<VFS>>, reply_right: Option<SendRight>, right: Option<pmos::ipc::SendRight>, mountpoint: String, root_inode: u64) {
    if mountpoint != "/" {
        println!("VFSd: Only mounting at / is supported for now");
        mount_filesystem_reply(reply_right, -libc::ENOSYS);
        return;
    }

    if right.is_none() {
        mount_filesystem_reply(reply_right, -libc::EINVAL);
        return;
    }
    let right = right.unwrap();

    let mut vfs = vfs.borrow_mut();

    if vfs.root_vnode.is_some() {
        mount_filesystem_reply(reply_right, -libc::EEXIST);
        return;
    }

    if !matches!(right, SendRight::Many(_)) {
        mount_filesystem_reply(reply_right, -libc::EINVAL);
        return;
    }

    println!("VFSd: Mounting filesystem at {}", mountpoint);

    let fs = Filesystem {
        right: Some(right),
        mountpoint,
    };
    let fs = Rc::new(RefCell::new(fs));
    vfs.filesystems.push(fs);

    let vnode = VNode {
        parent_fs: vfs.filesystems.last().unwrap().clone(),
        parent: None,
        inode: root_inode,
        name: "/".to_string(),
        is_dir: true,
    };
    let vnode = Rc::new(RefCell::new(vnode));
    vfs.vnodes.insert(root_inode, vnode.clone());

    vfs.root_vnode = Some(vnode);

    mount_filesystem_reply(reply_right, 0);
}

fn open_file(vfs: Rc<RefCell<VFS>>, reply_right: Option<SendRight>, path: String) {
    todo!();
}

async fn handle_ipc(executor: Executor) {
    let mut stream = async_helpers::create_named_stream(executor, "/pmos/vfsd")
        .await
        .unwrap();

    println!("Registered /pmos/vfsd...");

    let vfs = Rc::new(RefCell::new(VFS {
        filesystems: Vec::new(),
        vnodes: BTreeMap::new(),
        root_vnode: None,
    }));

    while let Some(mut message) = stream.next().await {
        match message.deserialize() {
            pmos::ipc_msgs::Message::IPCMountFS(msg) => {
                mount_filesystem(vfs.clone(), message.reply_right.take(), message.other_rights[0].take(), msg.path, msg.root_fd);
            }
            pmos::ipc_msgs::Message::IPCOpen(msg) => {
                open_file(vfs.clone(), message.reply_right.take(), msg.path);
            }
            _ => {
                println!("Received unknown message");
            }
        }
    }

    process::exit(0);
}

fn main() {
    println!("Hello from vfsd!");

    let executor = Executor::new();
    executor.spawn(handle_ipc(executor.clone()));
    executor.run();
}
