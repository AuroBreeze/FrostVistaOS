use crate::ffi;
use core::alloc::{GlobalAlloc, Layout};
use core::ptr::null_mut;

/// kalloc - Allocate a page from the kernel heap
/// Returns success: A pointer to the allocated page
/// Returns failure: 0
pub fn kalloc() -> *mut u8 {
    // SAFETY: The C function returns a valid pointer.
    unsafe { ffi::fv_page_alloc() as *mut u8 }
}

/// kfree - Free a page frome kalloc
/// Returns: void
pub fn kfree(page: *mut u8) {
    // SAFETY: The C function does not return.
    unsafe { ffi::fv_page_free(page as *mut _) }
}

struct PageAllocator;

unsafe impl GlobalAlloc for PageAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        if layout.size() > 4096 || layout.align() > 4096 {
            return null_mut();
        }

        crate::page::kalloc()
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        crate::page::kfree(ptr);
    }
}

#[global_allocator]
static ALLOCATOR: PageAllocator = PageAllocator;
