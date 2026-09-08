use core::ffi::c_char;
use core::fmt::{self, Write};

use crate::ffi;

const LOG_BUFFER_SIZE: usize = 256;

struct LogBuffer {
    bytes: [u8; LOG_BUFFER_SIZE],
    len: usize,
}

impl LogBuffer {
    const fn new() -> Self {
        Self {
            bytes: [0; LOG_BUFFER_SIZE],
            len: 0,
        }
    }

    fn ptr(&self) -> *const c_char {
        self.bytes.as_ptr() as *const c_char
    }

    fn finish(&mut self) {
        self.bytes[self.len.min(LOG_BUFFER_SIZE - 1)] = 0;
    }
}

impl Write for LogBuffer {
    fn write_str(&mut self, text: &str) -> fmt::Result {
        let available = LOG_BUFFER_SIZE - 1 - self.len;
        let bytes = text.as_bytes();
        let count = bytes.len().min(available);

        self.bytes[self.len..self.len + count].copy_from_slice(&bytes[..count]);
        self.len += count;

        if count == bytes.len() {
            Ok(())
        } else {
            Err(fmt::Error)
        }
    }
}

pub fn log(level: u32, args: fmt::Arguments<'_>) {
    let mut buffer = LogBuffer::new();

    let _ = buffer.write_fmt(args);
    buffer.finish();

    // SAFETY: The buffer remains valid during the C call and is NUL-terminated.
    unsafe {
        ffi::fv_log_write(level, buffer.ptr());
    }
}

pub fn panic(args: fmt::Arguments<'_>) -> ! {
    let mut buffer = LogBuffer::new();

    let _ = buffer.write_fmt(args);
    buffer.finish();

    // SAFETY: The C function does not return, and the buffer remains valid for the duration of the call.
    unsafe {
        ffi::fv_panic_write(buffer.ptr());
    }
}
