use core::ffi::{c_char, c_void};

unsafe extern "C" {
    pub fn fv_log_write(level: u32, data: *const c_char);

    pub fn fv_panic_write(data: *const c_char) -> !;

    pub fn fv_page_alloc() -> *mut c_void;

    pub fn fv_page_free(page: *mut c_void);
}
