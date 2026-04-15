#![feature(new_range_api)]

use pmos::error;

use pmos::ipc_runner::Executor;
use pmos::async_helpers;
use futures::StreamExt;

async fn handle_ipc(executor: Executor) {
    todo!();
    // while let Some(message) = stream.next().await {
    //     println!("Got message in vfsd!")
    // }
}

fn main() {
    println!("Hello from ext4!");

    let executor = Executor::new();
    executor.spawn(handle_ipc(executor.clone()));
    executor.run();
}
