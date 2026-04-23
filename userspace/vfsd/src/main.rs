#![feature(new_range_api)]

use pmos::ipc_runner::Executor;
use pmos::async_helpers;
use futures::StreamExt;

async fn handle_ipc(executor: Executor) {
    let mut stream = async_helpers::create_named_stream(executor, "/pmos/vfsd")
        .await
        .unwrap();

    println!("Registered /pmos/vfsd...");

    while let Some(message) = stream.next().await {
        println!("Got message in vfsd!")
    }
}

fn main() {
    println!("Hello from vfsd!");

    let executor = Executor::new();
    executor.spawn(handle_ipc(executor.clone()));
    executor.run();
}
