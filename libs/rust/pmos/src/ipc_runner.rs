use std::{
    future::Future,
    pin::Pin,
    task::{Context, Poll},
    rc::Rc,
    rc::Weak,
    cell::RefCell,
    mem,
    collections::BTreeMap,
    collections::VecDeque,
};

use super::error::Error;

use super::ipc::{
    RecieveOnceRight,
    RecieveManyRight,
    SendRight,
    Right,
    IPCPort
};

use std::task::{RawWaker, RawWakerVTable, Waker};

use futures::Stream;

struct Task {
    future: Pin<Box<dyn Future<Output = ()>>>,
    enqueued: bool,
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
type TaskRef = Rc<RefCell<Task>>;

pub(crate) struct ExecutorState {
    runnable: VecDeque<TaskId>,
    tasks: BTreeMap<TaskId, TaskRef>,

    rights: BTreeMap<Right, Rc<RefCell<RightEndpointState>>>,

    port: IPCPort,

    next_task_id: TaskId,

    message: Option<super::ipc::Message>,
}

impl ExecutorState {
    fn push_message(
        &mut self,
        endpoint_rc: &Rc<RefCell<RightEndpointState>>,
        msg: super::ipc::Message
    ) -> Option<Waker> {
        let mut endpoint = endpoint_rc.borrow_mut();

        assert!(endpoint.message.is_none(),
            "Right {:?} already has a message",
            endpoint.right
        );
        endpoint.message = Some(msg);

        if let RightStorage::Once(_) = endpoint.right {
            let mut right = RightStorage::None;
            mem::swap(&mut endpoint.right, &mut right);
            let id = right.get_id().unwrap();
            mem::forget(right);

            self.rights.remove(&id);
        }

        // TODO: The right deletion notification handling should be done here

        if let Some(waker) = endpoint.waker.take() {
            Some(waker)
        } else {
            None
        }
    }

    fn default_handler(&self, _msg: super::ipc::Message) {
        // Do nothing
    }

    pub(crate) fn get_port(&self) -> &IPCPort {
        &self.port
    }

    fn enqueue(&mut self, task_id: TaskId) {
        if let Some(task_rc) = self.tasks.get(&task_id) {
            let mut task = task_rc.borrow_mut();
            if !task.enqueued {
                task.enqueued = true;
                self.runnable.push_back(task_id);
            }
        }
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
                tasks: BTreeMap::new(),
                rights: BTreeMap::new(),
                port,
                next_task_id: 0,
                message: None,
            })),
        }
    }

    pub fn run(&self) {
        loop {
            let task_id = {
                self.state.borrow_mut().runnable.pop_front()
            };

            if let Some(task_id) = task_id {
                self.poll_task(task_id);
                continue;
            }

            self.poll_messages();
        }
    }

    pub fn spawn<F> (&self, future: F) -> TaskId
    where
        F: Future<Output = ()> + 'static,
    {
        let mut state = self.state.borrow_mut();

        let task_id = state.next_task_id;
        state.next_task_id += 1;

        state.tasks.insert(
            task_id,
            Rc::new(RefCell::new(Task {
                future: Box::pin(future),
                enqueued: false 
            })),
        );

        state.runnable.push_back(task_id);
        task_id
    }

    pub fn send_message_reply_once(
        &self,
        msg: &impl super::ipc_msgs::Serializable,
        right: &mut Option<SendRight>,
        include_rights: &mut [Option<SendRight>; 4],
    ) -> Result<OnceReciever, (Error, u64)> {
        let result = super::ipc::send_message_reply_once(msg, right, &self.state.borrow().port, include_rights);

        result.map(|right| OnceReciever::from_right(right, self.clone()))
    }

    fn create_waker(&self, task_id: TaskId) -> Waker {
        let waker = Rc::new(TaskWaker {
            task_id,
            executor: Rc::downgrade(&self.state),
        });

        unsafe {
            Waker::from_raw(RawWaker::new(
                Rc::into_raw(waker) as *const (),
                &VTABLE,
            ))
        }
    }

    fn poll_task(&self, task_id: TaskId) {
        let waker = self.create_waker(task_id);
        let mut cx = Context::from_waker(&waker);

        let task_rc = {
            let state = self.state.borrow();
            state.tasks.get(&task_id).cloned().expect("missing task")
        };

        let done = {
            let mut task = task_rc.borrow_mut();
            task.enqueued = false;
            task.future.as_mut().poll(&mut cx).is_ready()
        };

        if done {
            self.state.borrow_mut().tasks.remove(&task_id);
        }
    }

    fn poll_messages(&self) {
        let mut waker = None;

        let mut state = self.state.borrow_mut();
        if let Some(msg) = state.message.take() {
            let right = msg.sent_with_right;
            if let Some(endpoint_rc) = state.rights.get(&right).cloned() {
                let has_msg = endpoint_rc.borrow().message.is_some();
                if has_msg {
                    panic!("Right {:?} already has a message, deadlock", right);
                }

                waker = state.push_message(&endpoint_rc, msg);
            } else {
                state.default_handler(msg);
            }
        } else {
            let msg = state.port.pop_front_blocking();
            
            if let Some(endpoint_rc) = state.rights.get(&msg.sent_with_right).cloned() {
                let has_msg = endpoint_rc.borrow().message.is_some();
                if has_msg {
                    state.message = Some(msg);
                    return;
                }

                waker = state.push_message(&endpoint_rc, msg);
            } else {
                state.default_handler(msg);
            }
        }

        mem::drop(state);

        if let Some(waker) = waker {
            waker.wake();
        }
    }
}

struct TaskWaker {
    task_id: TaskId,
    executor: Weak<RefCell<ExecutorState>>,
}

impl TaskWaker {
    fn wake_task(&self) {
        if let Some(exec) = self.executor.upgrade() {
            let mut exec = exec.borrow_mut();
            exec.enqueue(self.task_id);
        }
    }
}

unsafe fn clone(data: *const ()) -> RawWaker {
    let rc = Rc::<TaskWaker>::from_raw(data as *const TaskWaker);

    let cloned = rc.clone();

    // don't drop original
    let _ = Rc::into_raw(rc);

    RawWaker::new(
        Rc::into_raw(cloned) as *const (),
        &VTABLE,
    )
}

unsafe fn wake(data: *const ()) {
    let rc = Rc::<TaskWaker>::from_raw(data as *const TaskWaker);

    rc.wake_task();
    // rc is dropped here
}

unsafe fn wake_by_ref(data: *const ()) {
    let rc = Rc::<TaskWaker>::from_raw(data as *const TaskWaker);

    rc.wake_task();

    // don't drop it
    let _ = Rc::into_raw(rc);
}

unsafe fn drop(data: *const ()) {
    let _ = Rc::<TaskWaker>::from_raw(data as *const TaskWaker);
}

static VTABLE: RawWakerVTable =
    RawWakerVTable::new(clone, wake, wake_by_ref, drop);