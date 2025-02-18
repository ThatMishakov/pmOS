use core::ffi::c_int;
use std::num::NonZero;

#[derive(Clone, Copy, PartialEq, Eq)]
pub struct Error(NonZero<i32>);

const ERRNO_MAX: c_int = 83;

impl Error {
    pub(super) fn from_errno(errno: c_int) -> Error {
        if errno >= ERRNO_MAX || errno <= 0 {
            Error(NonZero::new(libc::EINVAL as i32).unwrap())
        } else {
            Error(NonZero::new(errno as i32).unwrap())
        }
    }

    pub(super) fn from_result_t(result: super::system::ResultT) -> Error {
        Self::from_errno(-result.0 as c_int)
    }
}

impl Error {
    pub fn to_string(&self) -> &'static str {
        unsafe { std::ffi::CStr::from_ptr(libc::strerror(self.0.get())).to_str().unwrap() }
    }
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.to_string())
    }
}

impl std::fmt::Debug for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Error({}): {}", self.0, self.to_string())
    }
}