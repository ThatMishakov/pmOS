#![feature(inherent_str_constructors)]

pub mod ipc;
pub mod ipc_msgs;
pub mod system;
pub mod error;
pub mod mem_object;
pub mod pmbus;

// #[cfg(test)]
// mod tests {
//     use super::*;

//     #[test]
//     fn it_works() {
//         let result = add(2, 2);
//         assert_eq!(result, 4);
//     }
// }
