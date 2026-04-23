use super::error::Error;

#[repr(transparent)]
#[derive(Clone, Copy)]
pub struct ResultT(pub i64);

impl ResultT {
    pub fn new(val: i64) -> Self {
        ResultT(val)
    }

    pub fn success(self) -> bool {
        self.0 == 0
    }

    pub fn result(self) -> Result<(), Error> {
        if !self.success() {
            Err(Error::from_result_t(self))
        } else {
            Ok(())
        }
    }
}
