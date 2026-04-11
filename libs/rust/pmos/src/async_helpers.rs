use super::ipc_runner::Executor;

async fn create_named_stream(executor: Executor, name: &str) -> Result<ManyReciever, Error> {
    let msg = IPCNameRight {
        flags: 0,
        name,
    };

    let right = create_right_sendmany(executor.get_port())?;
    let stream = ManyReciever::from_right(right.1, executor.clone());

    let mut other_rights = [right.0, None, None, None];

    let msg = executor.send_message_reply_once(executor, &msg, right0, &mut other_rights)?;
    let msg = await msg;

    if let Some(IPCNameRightReply(msg)) = msg {
        if msg.result == 0 {
            Ok(stream)
        } else {
            Err(Error::from_errno(-msg.result))
        }
    } else {
        Err(Error::from_errno(libc::EFAULT))
    }
}