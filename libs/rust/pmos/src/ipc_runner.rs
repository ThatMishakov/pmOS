use std::{
    future::Future,
    pin::Pin,
    task::{Context, Poll},
    rc::Rc,
    rc::Weak,
    cell::RefCell,
    mem,
    task::Waker,
    collections::HashMap,
    collections::VecDeque,
};

use super::error::Error;

use super::ipc::{
    Port,
    RecieveOnceRight,
    RecieveManyRight,
    SendOnceRight,
    SendManyRight,
    SendRight,
    Right,
    IPCPort
};

use futures::Stream;

struct Task {
    future: Pin<Box<dyn Future<Output = ()>>>,
}

impl Task {
    fn poll(&mut self, cx: &mut Context<'_>) -> Poll<()> {
        self.future.as_mut().poll(cx)
    }
}

pub struct ManyReciever {
    state: Rc<RefCell<RightEndpointState>>,
    executor: Weak<RefCell<ExecutorState>>,
}

impl Stream for ManyReciever {
    type Item = super::ipc::Message;

    fn poll_next(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        let state = self.state.clone();
        let mut this = state.borrow_mut();
        
        if let Some(msg) = this.message.take() {
            Poll::Ready(Some(msg))
        } else {
            match this.right {
                RightStorage::None => Poll::Ready(None),
                _ => {
                    this.waker = Some(cx.waker().clone());
                    Poll::Pending
                }
            }
        }
    }
}

pub struct OnceReciever {
    state: Rc<RefCell<RightEndpointState>>,
    executor: Weak<RefCell<ExecutorState>>,
}

impl Future for OnceReciever {
    type Output = Option<super::ipc::Message>;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        let state = self.state.clone();
        let mut this = state.borrow_mut();
        
        if let Some(msg) = this.message.take() {
            Poll::Ready(Some(msg))
        } else {
            match this.right {
                RightStorage::None => Poll::Ready(None),
                _ => {
                    this.waker = Some(cx.waker().clone());
                    Poll::Pending
                }
            }
        }
    }
}

impl OnceReciever {
    pub(crate) fn from_right(right: RecieveOnceRight, executor: Executor) -> Self {
        let id = right.get_id();
        let state = Rc::new(RefCell::new(RightEndpointState {
            right: RightStorage::Once(right),
            message: None,
            waker: None,
        }));

        executor.state.borrow_mut().rights.insert(id, state.clone());

        Self {
            state,
            executor: Rc::downgrade(&executor.state),
        }        
    }
}

impl ManyReciever {
    pub(crate) fn from_right(right: RecieveManyRight, executor: Executor) -> Self {
        let id = right.get_id();
        let state = Rc::new(RefCell::new(RightEndpointState {
            right: RightStorage::Many(right),
            message: None,
            waker: None,
        }));

        executor.state.borrow_mut().rights.insert(id, state.clone());

        Self {
            state,
            executor: Rc::downgrade(&executor.state),
        }        
    }
}

#[derive(Debug)]
enum RightStorage {
    None,
    Once(RecieveOnceRight),
    Many(RecieveManyRight),
}

impl RightStorage {
    fn get_id(&self) -> Option<Right> {
        match self {
            Self::Once(r) => Some(r.get_id()),
            Self::Many(r) => Some(r.get_id()),
            Self::None => None,
        }
    }
}

struct RightEndpointState {
    right: RightStorage,
    message: Option<super::ipc::Message>,
    waker: Option<Waker>,
}

type TaskId = u64;

struct ExecutorState {
    runnable: VecDeque<TaskId>,
    tasks: HashMap<TaskId, Task>,

    rights: HashMap<Right, Rc<RefCell<RightEndpointState>>>,

    port: IPCPort,

    next_task_id: TaskId,

    message: Option<super::ipc::Message>,
}

impl ExecutorState {
    fn push_message(
        &mut self,
        endpoint_rc: &Rc<RefCell<RightEndpointState>>,
        msg: super::ipc::Message
    ) {
        let mut endpoint = endpoint_rc.borrow_mut();

        assert!(endpoint.message.is_none(),
            "Right {:?} already has a message",
            endpoint.right
        );
        endpoint.message = Some(msg);

        if let RightStorage::Once(_) = endpoint.right {
            let mut right = RightStorage::None;
            mem::swap(&mut endpoint.right, &mut right);
            mem::forget(right);

            self.rights.remove(&endpoint.right.get_id().unwrap());
        }

        // TODO: The right deletion notification handling should be done here

        if let Some(waker) = endpoint.waker.take() {
            waker.wake();
        }
    }

    fn default_handler(&self, _msg: super::ipc::Message) {
        // Do nothing
    }

    fn poll_messages(&mut self) {
        if let Some(msg) = self.message.take() {
            let right = msg.sent_with_right;
            if let Some(endpoint_rc) = self.rights.get(&right).cloned() {
                let has_msg = endpoint_rc.borrow().message.is_some();
                if has_msg {
                    panic!("Right {:?} already has a message, deadlock", right);
                }

                self.push_message(&endpoint_rc, msg);
            } else {
                self.default_handler(msg);
            }
            return;
        }


        let msg = self.port.pop_front_blocking();
        
        if let Some(endpoint_rc) = self.rights.get(&msg.sent_with_right).cloned() {
            let has_msg = endpoint_rc.borrow().message.is_some();
            if has_msg {
                self.message = Some(msg);
                return;
            }

            self.push_message(&endpoint_rc, msg);
        } else {
            self.default_handler(msg);
        }
    }

    fn poll_task(&mut self, task_id: TaskId) {
        let waker = self.create_waker(task_id);
        let mut cx = Context::from_waker(&waker);

        let done = self.tasks.get_mut(&task_id).unwrap().future.as_mut().poll(&mut cx).is_ready();
        if done {
            self.tasks.remove(&task_id);
        }
    }

    pub(crate) fn get_port(&self) -> &IPCPort {
        &self.port
    }
}

#[derive(Clone)]
pub struct Executor {
    pub(crate) state: Rc<RefCell<ExecutorState>>,
}

impl Executor {
    pub fn new() -> Self {
        let port = IPCPort::new().unwrap();
        Self {
            state: Rc::new(RefCell::new(ExecutorState {
                runnable: VecDeque::new(),
                tasks: HashMap::new(),
                rights: HashMap::new(),
                port,
                next_task_id: 0,
                message: None,
            })),
        }
    }

    fn run(&self) {
        loop {
            let task_id = {
                self.state.borrow_mut().runnable.pop_front()
            };

            while let Some(task_id) = task_id {
                self.state.borrow_mut().poll_task(task_id);
                continue;
            }

            self.state.borrow_mut().poll_messages();
        }
    }

    fn spawn<F> (&self, future: F) -> TaskId
    where
        F: Future<Output = ()> + 'static,
    {
        let mut state = self.state.borrow_mut();

        let task_id = state.next_task_id;
        state.next_task_id += 1;

        state.tasks.insert(task_id, Task { future: Box::pin(future) });
        state.runnable.push_back(task_id);
        task_id
    }

    pub fn send_message_reply_once(
        &self,
        msg: &impl super::ipc_msgs::Serializable,
        right: &mut Option<SendRight>,
        include_rights: &mut [Option<SendRight>; 4],
    ) -> Result<OnceReciever, (Error, u64)> {
        super::ipc::send_message_reply_once(msg, right, &self.state.borrow().port, include_rights)
            .map(|right| OnceReciever::from_right(right, self.clone()))
    }
}