use super::ipc_runner::{
    Executor,
    ManyReciever,
};

use crate::error::Error;

use crate::ipc::{
    get_right0,
    SendManyRight,
    SendRight,
};

use crate::ipc_msgs::{
    IPCNameRight,
    IPCGetNamedRight,
};

use crate::ipc_msgs;

pub async fn create_named_stream(executor: Executor, name: &str) -> Result<ManyReciever, Error> {
    let msg = IPCNameRight {
        flags: 0,
        name,
    };

    let right = executor.state.borrow().get_port().create_right_sendmany()?;
    let stream = ManyReciever::from_right(right.1, executor.clone());

    let mut other_rights = [Some(right.0.into()), None, None, None];
    let mut right0 = Some(get_right0().into());

    let msg = executor.send_message_reply_once(&msg, &mut right0, &mut other_rights).map_err(|e| e.0)?.await;
    msg.ok_or(Error::from_errno(libc::EINTR))
        .and_then(|msg| {
            match msg.deserialize() {
                ipc_msgs::Message::IPCNameRightReply(msg) => if msg.result == 0 {
                    Ok(stream)
                } else {
                    Err(Error::from_errno(-msg.result))
                }
                _ => Err(Error::from_errno(libc::EINTR))
            }
        })
}

pub async fn get_named_right(executor: Executor, name: &str) -> Result<SendManyRight, Error> {
    let msg = IPCGetNamedRight {
        flags: 0,
        name,
    };

    let mut right0 = Some(get_right0().into());
    let mut other_rights = [None, None, None, None];

    let msg = executor.send_message_reply_once(&msg, &mut right0, &mut other_rights).map_err(|e| e.0)?.await;

    msg.ok_or(Error::from_errno(libc::EINTR))
        .and_then(|mut msg| {
            let right = msg.other_rights[0].take();
                
            match msg.deserialize() {
                ipc_msgs::Message::IPCNamedRightNotification(msg) => if msg.result == 0 {
                    right.ok_or(Error::from_errno(libc::EFAULT))
                        .and_then(|r| match r {
                            SendRight::Many(r) => Ok(r),
                            _ => Err(Error::from_errno(libc::EFAULT))
                        })
                } else {
                    Err(Error::from_errno(-msg.result))
                }
                _ => {
                    eprintln!("get_named_right: Unexpected message type received in response to IPCGetNamedRight");
                    Err(Error::from_errno(libc::EINTR))
                }
            }
        })
}