#![no_std]

mod console;
mod ffi;
mod page;

#[macro_export]
macro_rules! rust_log {
    ($level:expr, $($arg:tt)*) => {
        $crate::console::log($level, core::format_args!($($arg)*))
    };
}

#[macro_export]
macro_rules! rust_panic {
    ($($arg:tt)*) => {
        $crate::console::panic(core::format_args!($($arg)*))
    };
}

#[unsafe(no_mangle)]
pub extern "C" fn fv_rust_init() {
    rust_log!(2, "Rust initialized\n");
}

#[panic_handler]
fn panic_handler(info: &core::panic::PanicInfo<'_>) -> ! {
    console::panic(core::format_args!("Rust panic: {}\n", info))
}
