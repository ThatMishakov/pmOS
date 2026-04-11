use super::ipc_runner::{
    Executor,
    ManyReciever,
};

use crate::error::Error;

use crate::ipc::get_right0;

use crate::ipc_msgs::{
    IPCNameRight,
    IPCNameRightReply,
    Message,
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

    let msg = executor.send_message_reply_once(&msg, &mut right0, &mut other_rights).map_err(|e| e.0)?;
    msg.await
        .ok_or(Error::from_errno(libc::EFAULT))
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