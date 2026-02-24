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
use std::range::Bound;
use std::rc::Rc;

struct ObjectInfo {
    sequence_id: u64,
    properties: pmbus::ObjectProperties,
    name: String,
    right: SendManyRight,
}

#[derive(Default)]
struct State {
    properties: BTreeMap<u64, Rc<RefCell<ObjectInfo>>>,
    next_sequence_id: u64,
    main_port: ipc::Port,
    pending_filters: BTreeMap<ipc::Right, (Option<SendRight>, AnyFilter)>,
}

fn publish_object_error(error: i32, reply_right: SendRight) {
    let mut msg = ipc_msgs::IPCBusPublishObjectReply::new();
    msg.result = -error;

    _ = ipc::send_message_right_consume(&msg, reply_right, None, [const { None }; 4]);
}

fn publish_object_success(reply_right: &mut Option<SendRight>, seq_id: u64) -> Result<(), i32> {
    let mut msg = ipc_msgs::IPCBusPublishObjectReply::new();
    msg.sequence_number = seq_id;

    ipc::send_message_right(&msg, reply_right, None, &mut [const { None }; 4])
        .map(|_| ())
        .map_err(|(e, _)| e.get())
}

struct TaskGroup {
    id: u64,
    sequences: BTreeSet<u64>,
}

impl TaskGroup {
    fn new(id: u64, port: ipc::Port) -> Result<Rc<RefCell<TaskGroup>>, i32> {
        task_group::set_group_notifier_mask(
            id,
            port,
            task_group::NotificationMask::GROUP_DESTROYED,
            0,
        )
        .map_err(|e| e.get())
        .map(|_| {
            Rc::new(RefCell::new(TaskGroup {
                id: id,
                sequences: BTreeSet::new(),
            }))
        })
    }
}

fn publish_object(
    object: ipc_msgs::IPCBusPublishObject,
    reply_right: Option<SendRight>,
    object_right: Option<SendRight>,
    message: &ipc::Message,
    state: &mut State,
) {
    if reply_right.is_none() {
        println!(
            "Recieved PMBusPublishObject from task {} with no reply right!",
            message.sender
        );
        return;
    }

    let mut reply_right = reply_right;

    let object_right = match object_right {
        Some(SendRight::Many(m)) => m,
        Some(SendRight::Once(_)) => {
            // No object right...
            publish_object_error(libc::ENOENT, reply_right.unwrap());
            return;
        }
        None => {
            publish_object_error(libc::ENOENT, reply_right.unwrap());
            return;
        }
    };

    match pmbus::PMBusObject::deserialize(&object.object_data) {
        Ok(object) => {
            println!("{:?}", &object);

            let id = state.next_sequence_id;
            let info = ObjectInfo {
                sequence_id: id,
                properties: object.properties,
                name: object.name.into_string(),
                right: object_right,
            };

            if let Err(error) = publish_object_success(&mut reply_right, id) {
                publish_object_error(error, reply_right.unwrap());
                return;
            }

            let info = Rc::new(RefCell::new(info));
            state.properties.insert(id, info.clone());
            state.next_sequence_id += 1;

            new_object_stuff(info, state);
        }
        Err(e) => {
            publish_object_error(e.get(), reply_right.unwrap());
        }
    }
}

fn queue_request_object(filter: AnyFilter, state: &mut State, reply_right: SendRight) {
    let id = reply_right.get_id();

    // Kernel should give new right IDs every time, so there shouldn't be duplicates
    let result = state
        .pending_filters
        .insert(id, (Some(reply_right), filter));

    assert!(
        result.is_none(),
        "Expected new insert, got a duplicate (for reply right {})",
        id
    );
}

fn new_object_stuff(obj: Rc<RefCell<ObjectInfo>>, state: &mut State) {
    state.pending_filters.retain(|_, (right, f)| {
        if f.matches(&obj.borrow().properties) {
            let obj = obj.borrow();

            let id = obj.sequence_id;
            let id_after = id + 1;

            let r = ipc_msgs::IPCBusRequestObjectReply {
                flags: 0,
                result: 0,
                next_sequence_number: id_after,
                object: Some((&obj.name, &obj.properties)),
                object_id: id,
            };

            let clone = obj.right.clone().ok().map(|r| SendRight::from(r));
            if clone.is_none() {
                return false;
            }

            let mut other_rights = [clone, None, None, None];

            let result =
                ipc::send_message_right(&r, right, None, &mut other_rights).map_err(|(e, _)| e);
            if let Err(r) = result {
                println!("Error replying to the right request: {}", r);

                if r.get() == libc::ESRCH {
                    // Right went away, so don't remove the request...
                    return true;
                }
            }

            false
        } else {
            true
        }
    });
}

fn request_object(
    object: ipc_msgs::IPCBusRequestObject,
    state: &mut State,
    sender_task: u64,
    reply_right: Option<SendRight>,
) {
    if reply_right.is_none() {
        println!("pmbus: Recieved object filter request with no reply right!");
        return;
    }

    let mut reply_right = reply_right;

    if let Ok(req) = AnyFilter::deserialize(&object.filter_data) {
        // Try matching everything known, then reply or stash...
        let filter = req.0;
        let start_with = object.start_sequence_number;

        let mut it = state
            .properties
            .range((Bound::Included(start_with), Bound::Unbounded));

        let mut search_item = it.find(|(_idx, item)| !filter.matches(&item.borrow().properties));

        while let Some(item) = search_item {
            // Answer immediately

            let id_after = it.clone().next().map_or(state.next_sequence_id, |(i, _)| *i);

            let obj = item.1.borrow();

            let r = ipc_msgs::IPCBusRequestObjectReply {
                flags: 0,
                result: 0,
                next_sequence_number: id_after,
                object: Some((&obj.name, &obj.properties)),
                object_id: *item.0,
            };

            let mut other_rights = [const { None }; 4];
            other_rights[0] = obj.right.clone().ok().map(|r| SendRight::from(r));
            if other_rights[0].is_none() {
                search_item = it.find(|(_idx, item)| !filter.matches(&item.borrow().properties));
                continue;
            }

            let result =
                ipc::send_message_right(&r, &mut reply_right, None, &mut other_rights)
                    .map_err(|(e, _)| e);
            if let Err(e) = result {
                match e.get() {
                    libc::ENOENT => println!("pmbus error: Task {} requested object, but reply port went away...", sender_task),
                    libc::ESRCH => {
                        search_item = it.find(|(_idx, item)| !filter.matches(&item.borrow().properties));
                        continue;
                    },
                    e => println!("pmbus error: Error sending object to task {}, error {}", sender_task, e),
                }
            }
        }

        queue_request_object(filter, state, reply_right.unwrap());
    } else {
        println!("Failed to deserialize filters...")
    }
}

fn main() {
    let mut port = IPCPort::new().unwrap();
    let mut state = State {
        next_sequence_id: 1,
        main_port: port.get_id(),
        ..Default::default()
    };
    let (send_right, _reply_right) = port.create_right_sendmany().unwrap();
    send_right.name_self("/pmos/pmbus").unwrap();

    println!("pmbus port {}", port.get_id());

    loop {
        let mut msg = port.pop_front_blocking();

        let reply_right = msg
            .reply_right
            .take()
            .or_else(|| msg.other_rights[3].take());
        let extra_right = msg.other_rights[0].take();

        match msg.deserialize() {
            Message::IPCBusPublishObject(o) => {
                publish_object(o, reply_right, extra_right, &msg, &mut state);
            }
            Message::IPCBusRequestObject(o) => {
                request_object(o, &mut state, msg.sender, reply_right);
            }
            Message::Unknown => {
                let id = msg.get_known_id();
                println!("pmbus: Unknown message {:?}", id);
            }
        }
    }
}
