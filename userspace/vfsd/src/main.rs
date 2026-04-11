#![feature(new_range_api)]

use pmos::error;
use pmos::ipc;
use pmos::ipc::IPCPort;
use pmos::ipc::SendManyRight;
use pmos::ipc::SendRight;
use pmos::ipc_msgs;
use pmos::ipc_msgs::Message;
use pmos::pmbus;
use pmos::pmbus::AnyFilter;
use pmos::task_group;
use pmos::async_helpers;
use pmos::ipc_runner;
use std::cell::RefCell;
use std::collections::BTreeMap;
use std::collections::BTreeSet;
use std::ops::Bound;
use std::rc::Rc;

async fn handle_ipc(executor: Executor) {
    let stream = create_named_stream(executor, "/pmos/vfsd");
    while let Some(message) = stream.next().await {
        println!("Got message {message}")
    }
}

fn main() {
    println!("Hello from vfsd!");

    let executor = Executor::new();

}
