# Roadmap (v1.7.0 - Basic Syscalls and musl/BusyBox Compatibility)

v1.7.0 focuses on completing basic syscall behavior and improving musl and BusyBox compatibility on RISC-V 64 and LoongArch64.

The release should run the image-provided basic and BusyBox tests with their original commands and execution order while preserving the existing kernel regression baseline.

## Phase 1 - Basic Syscall Compatibility <!-- id: syscall-phase-1 -->

  - [ ] **I/O polling** <!-- id: syscall-io-polling -->: implement `ppoll` readiness checks, pipe close/error events, timeouts, signal masks, and wakeups so that the original BusyBox `while read` loop can run.
  - [ ] **Process lifecycle** <!-- id: syscall-process-lifecycle -->: validate `fork`/`clone`, `execve`, `wait4`, exit status, and background process handling with the image binaries. Document remaining stopped/continued-child limitations.
  - [ ] **File operations** <!-- id: syscall-file-operations -->: complete directory-relative open, create, truncate, append, seek, vectored I/O, access checks, unlink, rename, and directory removal behavior.
  - [ ] **File metadata and names** <!-- id: syscall-file-metadata -->: complete `fstat`/`newfstatat` metadata and flags, and align filename capacity across VFS, tmpfs, and EXT4 overlay paths.
  - [ ] **Time and timestamps** <!-- id: syscall-time-timestamps -->: implement `utimensat` and file timestamp updates needed by `touch`; validate time-related calls used by musl and BusyBox.
  - [ ] **Runtime interfaces** <!-- id: syscall-runtime-interfaces -->: preserve `execve` arguments and environment, including `PATH`; resolve required `gettid`, `sendfile`, and terminal `ioctl` behavior or validate working fallbacks.

## Phase 2 - musl and BusyBox Tests <!-- id: compatibility-phase-2 -->

  - [ ] **musl basic tests** <!-- id: compatibility-musl-basic -->: run the image-provided basic syscall suite on both architectures, check return values and error paths, and track each remaining failure.
  - [ ] **Original BusyBox commands** <!-- id: compatibility-busybox-commands -->: use each image's `busybox_testcode.sh` and `busybox_cmd.txt` as the test source of truth. Preserve all 55 command-list entries, shell context, working directory, and the `test.txt` file lifecycle.
  - [ ] **System information** <!-- id: compatibility-system-information -->: provide the process, memory, filesystem, and mount information required by `ps`, `free`, `uptime`, `df`, and `du`.
  - [ ] **Device and log interfaces** <!-- id: compatibility-device-interfaces -->: provide kernel log access for `dmesg`, RTC operations for `hwclock`, and the terminal behavior required by the shipped BusyBox applets.
  - [ ] **Mount test environment** <!-- id: compatibility-mount-environment -->: prepare the FAT partition and `/dev/vda2` device expected by the basic mount tests, together with partition discovery and filesystem support. Track missing fixtures separately from syscall failures.

## Phase 3 - Regression and Release Quality <!-- id: compatibility-quality-phase-3 -->

  - [ ] **Architecture test matrix** <!-- id: compatibility-quality-matrix -->: validate musl basic and original BusyBox tests on RISC-V 64 and LoongArch64 while retaining EasyFS/tmpfs, process, memory, signal, and user-application regressions.
  - [ ] **Failure diagnostics** <!-- id: compatibility-quality-diagnostics -->: save per-command results and logs by architecture, image, libc, and boot mode. Treat missing results, timeouts, early termination, and failed commands as failures even when the outer script returns zero; retain the original `false` exception.
  - [ ] **Compatibility baseline** <!-- id: compatibility-quality-baseline -->: record image hashes and publish syscall/command support and validation results. Resolve or explicitly defer remaining kernel, fixture, and applet gaps before release.
