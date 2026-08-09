# Spurious S-Mode External Interrupt — RESOLVED

> Moved out of `releases.md` on 2026-06-27 to keep the roadmap focused.
> Originally documented as a v1.1-era release blocker; the workaround was later disabled and the hang did not recur.

> **Status: RESOLVED (2026-07-27).** Root cause found and fixed in `arch/riscv/trap/mtrap.c`. See "Root Cause" and "Fix" below.

## Symptom

After VirtIO block requests complete, the kernel can enter a repeated S-mode external interrupt path where `plic_claim_interrupt(context)` returns `0` while `sip.SEIP` remains set (`sip = 0x200`). With `irq == 0` handled by a plain `return` (xv6 style), the CPU retraps on the stale `SEIP` forever: the counter grows to ~400k spurious interrupts, no real interrupt (virtio/UART/timer) is ever claimed, and the system hangs.

## Evidence Collected (during investigation)

- `irq == 0` after a completed VirtIO request, repeatedly, up to hundreds of thousands of times.
- `sip = 0x200` (`SEIP` pending) while PLIC pending register = `0x0` and VirtIO interrupt status = `0x0`.
- PLIC enable/threshold were correct (`en=0x402`, `thr=0x0`).
- Claiming PLIC contexts all returned `0`.
- Writing `sip` did not clear `SEIP` (QEMU does not allow S-mode software to clear `SEIP`).
- `plic_force_update` (rewriting the threshold register) did not clear `SEIP` either.
- Masking `SEIE` on `irq == 0` and re-enabling it from the timer path allowed progress (workaround, not a fix).
- Control experiment: xv6-riscv on the same QEMU 11.0.3 was fine single-core — pointing to a guest-side difference, not a QEMU bug.

## Root Cause

QEMU computes `mip.SEIP` as `external_seip | software_seip` (`target/riscv/cpu.c`):

- `external_seip` follows the PLIC external interrupt line (cleared when the PLIC drains);
- `software_seip` is latched from whatever the guest writes into the `SEIP` bit of `mip` (`target/riscv/csr.c`, `rmw_mip64`):
  `env->software_seip = new_val & MIP_SEIP`.

The kernel's M-mode timer/`SBI_SET_TIMER` handling in `mtrap.c` set `STIP` with a **read-modify-write of the whole `mip` register**:

```c
w_mip(r_mip() | MIP_STIP);     // before the fix
w_mip(r_mip() & ~MIP_STIP);    // before the fix
```

Whenever this ran while a VirtIO interrupt was pending (`mip.SEIP == 1`), the read-modify-write wrote `SEIP = 1` back into `mip`. QEMU latched that bit into `software_seip`, so `mip.SEIP` stayed `1` forever — even after the PLIC drained (`pending = 0`, `vq_isr = 0`). The result was an infinite stream of spurious external interrupts with nothing claimable.

**Why xv6 was unaffected:** xv6's M-mode `timervec` writes a constant (`csrw sip, 2`, i.e. only the `SSIP` bit, `SEIP` bit = 0) and never read-modify-writes `mip`, so `software_seip` is never latched.

## Fix

`arch/riscv/trap/mtrap.c`: clear the `SEIP` bit before writing `mip` so the read-modify-write never latches a `1` into `software_seip`.

```c
// code == 7 (M-mode timer) branch:
uint64 mip_val = r_mip() & ~MIP_SEIP;   // never write SEIP back
w_mip(mip_val | MIP_STIP);

// SBI_SET_TIMER branch:
w_mip((r_mip() & ~MIP_SEIP) & ~MIP_STIP);
```

`arch/riscv/include/asm/trap.h`: added `MIP_SEIP (1UL << 9)`.

`arch/riscv/trap/trap.c`: the old `SEIE` mask/re-enable workaround and all diagnostic counters were removed; `irq == 0` now logs once (`LOG_DEBUG`) and returns, xv6-style.

## Verification

- `TEST=readloop` (a continuous multi-block read workload, the strongest trigger) no longer produces the spurious storm; the `[CTR] sp=` counter stays flat / the system completes.
- The fix works under the same QEMU 11.0.3 where the storm previously occurred.

## Key Takeaways

1. Read-modify-write of `mip`/`sip` is dangerous: bits with hardware semantics (e.g. `SEIP`) get latched by QEMU's `software_seip` mechanism. xv6's "write a constant" style is the safe pattern.
2. "PLIC pending == 0 but `SEIP` set" is impossible from the PLIC side alone; it means `software_seip` was latched by a guest `mip`/`sip` write.
3. Control experiments against xv6 on the same QEMU were decisive in ruling out a QEMU bug.
