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
use std::cell::RefCell;
use std::collections::BTreeMap;
use std::collections::BTreeSet;
use std::ops::Bound;
use std::rc::Rc;

fn main() {
    println!("Hello from vfsd!");
}
