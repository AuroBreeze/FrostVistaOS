#include "user.h"

void _start() {
    print("--- Testing Lazy Allocation in Kernel Space ---\n");

    // Test 1: Triggering copyin lazy allocation
    print("[Test 1] Triggering copyin lazy alloc...\n");
    
    // 1. Allocate 4KB of memory.
    // Due to lazy allocation, the kernel only updates the size,
    // but no actual physical page is mapped to this virtual address yet.
    char *buf_in = sbrk(4096);
    
    // 2. CRITICAL STEP: Do NOT access buf_in in user space!
    // We pass the unmapped pointer directly to the 'write' system call.
    // The kernel will call 'copyin' to read from this address.
    // If our kernel patch is correct, 'copyin' will detect the missing page
    // and allocate it on the fly, filling it with zeros.
    long ret = write(1, buf_in, 5); 
    
    if (ret == 5) {
        print("\n[Test 1] copyin passed! Kernel successfully handled the unmapped page.\n");
    } else {
        print("\n[Test 1] copyin failed!\n");
    }

    print("[Test 2] Triggering copyout lazy alloc...\n");
    
    // Allocate another unmapped page.
    char *buf_out = sbrk(4096);
    
    /*
    print("Please type something (press Enter): ");
    
    // The kernel's 'sys_read' will call 'copyout' to write your input into buf_out.
    // This will trigger the lazy allocation inside 'copyout'.
    read(0, buf_out, 10); 
    
    // Echo the input back to verify it actually worked.
    write(1, buf_out, 10); 
    print("\n[Test 2] copyout passed!\n");
    */

    print("--- All Kernel Lazy Allocation Tests Finished ---\n");
    exit(0);
}
