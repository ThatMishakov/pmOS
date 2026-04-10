use std::{
    future::Future,
    pin::Pin,
    task::{Context, Poll},
};

use super::ipc::{Port, RecieveRight, Right};
use alloc::collections::VecDeque;

struct Task {
    future: Pin<Box<dyn Future<Output = ()>>>,
}

impl Task {
    fn poll(&mut self, cx: &mut Context<'_>) -> Poll<()> {
        self.future.as_mut().poll(cx)
    }
}

struct ManyReciever {
    state: Rc<RefCell<IpcEndpointState>>,
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
            if let RightStorage::None = this.right {
                Poll::Ready(None)
            } else {
                Poll::Pending
            }
        }
    }
}

struct OnceReciever {
    state: Rc<RefCell<IpcEndpointState>>,
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
            if let RightStorage::None = this.right {
                Poll::Ready(None)
            } else {
                Poll::Pending
            }
        }
    }
}

enum RightStorage {
    None,
    Once(SendOnceRight),
    Many(SendManyRight),
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
    fn push_message(&mut self, endpoint: &mut RightEndpointState, msg: super::ipc::Message) {
        assert!(endpoint.message.is_none(), "Right {:?} already has a message", endpoint.right);
        endpoint.message = Some(msg);

        if let RightStorage::Once(_) = endpoint.right {
            forget!(endpoint.right);
            endpoint.right = RightStorage::None;

            self.rights.remove(&endpoint.right);
        }

        // TODO: The right deletion notification handling should be done here

        if let Some(waker) = self.waker.take() {
            waker.wake();
        }
    }

    fn default_handler(&self, _msg: super::ipc::Message) {
        // Do nothing
    }

    fn poll_messages(&mut self) {
        if let Some(msg) = self.message.take() {
            let right = msg.right;
            if let Some(endpoint) = self.rights.get(&right) {
                let mut endpoint = endpoint.borrow_mut();
                if endpoint.message.is_some() {
                    panic!("Right {:?} already has a message, deadlock", right);
                }

                endpoint.push_message(msg);
            } else {
                self.default_handler(msg);
            }
            return;
        }


        let msg = self.port.pop_front_blocking();
        
        if let Some(endpoint) = self.rights.get(&msg.right) {
            let mut endpoint = endpoint.borrow_mut();
            if endpoint.message.is_some() {
                self.message = Some(msg);
                return;
            }

            endpoint.push_message(msg);
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
}

pub struct Executor {
    state: Rc<RefCell<ExecutorState>>,
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
        let mut state = self.state.borrow_mut();
        loop {
            while let Some(task_id) = state.runnable.pop_front() {
                state.poll_task(task_id);
            }

            state.poll_messages();
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
}