use pmos::ipc::IPCPort;

fn main() {
    let mut port = IPCPort::new().unwrap();
    port.name_port("/pmos/pmbus");
    loop {
        let msg = port.pop_front_blocking();
        println!("Gotten a message in pmbus");
    }
}
