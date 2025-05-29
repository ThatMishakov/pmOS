use pmos::ipc;
use pmos::ipc::IPCPort;
use pmos::ipc_msgs;
use pmos::ipc_msgs::Message;
use pmos::pmbus;
use pmos::task_group;
use std::cell::RefCell;
use std::collections::BTreeMap;
use std::collections::BTreeSet;
use std::rc::Rc;
use std::rc::Weak;

struct ObjectInfo {
    sequence_id: u64,
    properties: pmbus::ObjectProperties,
    name: String,
    group: Rc<RefCell<TaskGroup>>,
    handle_port: u64,
}

#[derive(Default)]
struct State {
    properties: BTreeMap<u64, Rc<RefCell<ObjectInfo>>>,
    task_groups: BTreeMap<u64, Weak<RefCell<TaskGroup>>>,
    next_sequence_id: u64,
    main_port: ipc::Port,
}

fn publish_object_error(error: i32, reply_port: u64, user_arg: u64) {
    let mut msg = ipc_msgs::IPCBusPublishObjectReply::new();
    msg.result = -error;
    msg.user_arg = user_arg;

    _ = ipc::send_message(&msg, reply_port, None);
}

fn publish_object_success(reply_port: u64, user_arg: u64, seq_id: u64) -> Result<(), i32> {
    let mut msg = ipc_msgs::IPCBusPublishObjectReply::new();
    msg.sequence_number = seq_id;
    msg.user_arg = user_arg;

    ipc::send_message(&msg, reply_port, None).map_err(|e| e.get())
}

struct TaskGroup {
    id: u64,
    sequences: BTreeSet<u64>,
}

impl TaskGroup {
    fn new(id: u64, port: ipc::Port) -> Result<Rc<RefCell<TaskGroup>>, i32> {
        task_group::set_group_notifier_mask(id, port, task_group::NotificationMask::GROUP_DESTROYED, 0)
            .map_err(|e| e.get())
            .map(|_| {
                Rc::new(RefCell::new(TaskGroup {
                    id: id,
                    sequences: BTreeSet::new(),
                }))
            })
    }
}

fn create_find_task_group(
    task_group_id: u64,
    state: &mut State,
) -> Result<Rc<RefCell<TaskGroup>>, i32> {
    if let Some(weak) = state.task_groups.get(&task_group_id) {
        Ok(weak.upgrade().unwrap())
    } else {
        TaskGroup::new(task_group_id, state.main_port).map(|ptr| {
            state.task_groups.insert(task_group_id, Rc::downgrade(&ptr));
            ptr
        })
    }
}

fn new_object_stuff(sequence_id: u64, state: &mut State) {
    //println!("New object {sequence_id}");
}

fn publish_object(
    object: ipc_msgs::IPCBusPublishObject,
    message: &ipc::Message,
    state: &mut State,
) {
    let user_arg = object.user_arg;
    let reply_port = object.reply_port;
    match pmbus::PMBusObject::deserialize(&object.object_data) {
        Ok((object, task_group)) => {
            println!("{:?}", &object);

            if !task_group::is_task_of_group(message.sender, task_group) {
                publish_object_error(libc::EPERM, reply_port, user_arg);
                return;
            }

            let task_group_ptr;
            match create_find_task_group(task_group, state) {
                Ok(g) => {
                    task_group_ptr = g;
                }
                Err(i) => {
                    publish_object_error(i, reply_port, user_arg);
                    return;
                }
            }

            let id = state.next_sequence_id;
            let info = ObjectInfo {
                sequence_id: id,
                properties: object.properties,
                name: object.name.into_string(),
                group: task_group_ptr,
                handle_port: object.handle_port,
            };

            if let Err(error) = publish_object_success(reply_port, user_arg, id) {
                publish_object_error(error, reply_port, user_arg);
                return;
            }

            info.group.borrow_mut().sequences.insert(id);
            state.properties.insert(id, Rc::new(RefCell::new(info)));
            state.next_sequence_id += 1;

            new_object_stuff(id, state);
        }
        Err(e) => {
            publish_object_error(e.get(), object.reply_port, object.user_arg);
        }
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

    loop {
        let msg = port.pop_front_blocking();
        match msg.deserialize() {
            Message::IPCBusPublishObject(o) => {
                publish_object(o, &msg, &mut state);
            }
            Message::Unknown => {
                let id = msg.get_known_id();
                println!("pmbus: Unknown message {:?}", id);
            }
        }
    }
}
