# Spurious S-Mode External Interrupt — Historical Record

> Moved out of `releases.md` on 2026-06-27 to keep the roadmap focused.
> Originally documented as a v1.1-era release blocker; the workaround was later disabled and the hang did not recur. See the 2026-06-27 update at the end of this file.

> Status: unresolved root cause. The workaround is no longer in the code; the disappearance is not a confirmed fix.

## Symptom

After a VirtIO block request completes, the kernel can enter a repeated S-mode external interrupt path where `plic_claim_interrupt(context)` returns `0` while `sip.SEIP` remains set. The scheduler then cannot make progress and the system appears to hang with timer ticks or repeated empty external interrupts.

## Evidence Collected

- `irq == 0` after a completed VirtIO request.
- `sip = 0x200` (`SEIP` pending) while `sie = 0x220` (`STIE | SEIE`).
- PLIC pending register reported no pending source.
- VirtIO interrupt status was already clear.
- UART status did not indicate an RX/TX interrupt source.
- Claiming PLIC contexts `0`, `1`, and `2` all returned `0`.
- Writing `sip` did not clear `SEIP`.
- Temporarily masking `SEIE` on `irq == 0` and re-enabling it from the timer path allowed the kernel to make progress.

## Original Workaround (now disabled)

When PLIC claim returned `0`, the trap handler masked `SEIE` once. The timer interrupt path re-enabled `SEIE`, which broke the immediate external interrupt storm and gave the scheduler a chance to continue.

This was never a root-cause fix. It was a progress workaround for a stale or spurious external interrupt state where no claimable PLIC source existed. In `arch/riscv/trap/trap.c` the `SEIE` mask/re-enable calls were later commented out after testing showed the hang no longer recurred.

## Follow-Up Items

These are downgraded from "blocker" to "investigate when time permits":

- Revisit PLIC completion and external interrupt deassert ordering.
- Compare behavior under `BOOT=bare` and `BOOT=opensbi`.
- Check QEMU `virt` PLIC behavior and RISC-V privileged `sip.SEIP` semantics.
- Replace the workaround with a principled fix once the source of stale `SEIP` is understood.

## Update — 2026-06-27

During testing the `SEIE` mask/re-enable workaround was disabled in `trap.c` (commented out, not deleted) and the kernel boots and runs without the old hang. Tested multiple times; the high-frequency stall that motivated the workaround did not recur.

The root cause was never identified, so this is not a confirmed fix — it is a disappearance. Possible explanations include changes to the VirtIO completion path, the double `handle_page_fault` removal, or the EXT4 probe cleanup that landed between the original report and today.

Leave this document as the historical record. If the hang reappears, the workaround is the first thing to reintroduce.
