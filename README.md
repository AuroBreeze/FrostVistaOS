
# FrostVista OS / 霜见内核

FrostVista is a personal experimental OS kernel targeting **RISC-V**.

It is not meant to be a full-featured general-purpose OS.  
The goal is to build a **clear, readable, and hackable** playground for:

- low-level RISC-V
- memory management and paging
- traps, interrupts, and syscalls
- scheduling and simple filesystems
- later: real RISC-V boards 

This is a long-term project. Expect rough edges and frequent rewrites.

## Build & run

> Note: commands and paths here are placeholders.  
> Adjust them to match your actual toolchain and directory structure.

Requirements (planned):

- RISC-V cross toolchain (e.g. `riscv64-unknown-elf-gcc`)
- `make`
- `qemu-system-riscv64`

Typical flow:

```bash
# build kernel
make run
```

On success, you should see a short boot message from FrostVista over the serial console.

---

## Philosophy

* Prefer **clarity over cleverness**, and **structure over features**.
* Keep the kernel small enough to read and reason about.
* Treat this codebase as a long-term notebook for systems experiments.

If you are interested in OS dev, RISC-V, or low-level experiments, feel free to read, fork, or open an issue to discuss ideas.


