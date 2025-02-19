use pmos::ipc::IPCPort;
use pmos::ipc;
use pmos::ipc_msgs;
use pmos::ipc_msgs::Message;
use pmos::pmbus;

fn publish_object(object: ipc_msgs::IPCBusPublishObject, message: &ipc::Message) {
    let obj = pmbus::PMBusObject::deserialize(&object.object_data);

    println!("{:?}", &obj);
}

fn main() {
    let mut port = IPCPort::new().unwrap();
    port.name_port("/pmos/pmbus");
    loop {
        let msg = port.pop_front_blocking();
        match msg.deserialize() {
            Message::IPCBusPublishObject(o) => {
                publish_object(o, &msg);
            }
            Message::Unknown => {
                let id = msg.get_known_id();
                println!("pmbus: Unknown message {:?}", id);
            }
        }
    }
}
