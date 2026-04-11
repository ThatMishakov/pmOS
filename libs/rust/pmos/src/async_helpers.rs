use super::ipc_runner::Executor;

use crate::error::Error;

use crate::ipc_runner::{
    ManyReciever,
};

use crate::ipc_msgs::{
    IPCNameRight,
    IPCNameRightReply,
    Message,
};

async fn create_named_stream(executor: Executor, name: &str) -> Result<ManyReciever, Error> {
    let msg = IPCNameRight {
        flags: 0,
        name,
    };

    let right = executor.state.borrow().get_port().create_right_sendmany()?;
    let stream = ManyReciever::from_right(right.1, executor.clone());

    let mut other_rights = [Some(right.0.into()), None, None, None];
    let right0 = get_right0();

    let msg = executor.send_message_reply_once(&msg, right0, &mut other_rights).map_err(|e| e.0)?;
    let msg = msg.await;

    if let Some(ipc_msgs::Message::IPCNameRightReply(msg)) = msg {
        if msg.result == 0 {
            Ok(stream)
        } else {
            Err(Error::from_errno(-msg.result))
        }
    } else {
        Err(Error::from_errno(libc::EFAULT))
    }
}