# Android BIOS SWI live attribution (opt-in diagnostic)

This branch is based on the working Android on-screen HUD (engine PR #3), independent from the HALT reproducer (engine PR #5). It does **not** fix CPU HALT behavior, add HLE routines, or change the guest instruction/cycle flow. No ROM or BIOS is bundled.

## Why another diagnostic snapshot?

Moto G100, first Goku stage, previously observed: 35.9 FPS (~60% GBA speed), guest ~20 ms/frame, bridge 13.3 ms/frame, ~1.42 million bridged instructions/sec; HOT entry 0x00000008 had 488.9 ms, 22 calls and ~1.458 million instructions. Engine PR #5 demonstrated that multiple unsupported BIOS SWI services share that vector; its number is **not identified by an entry-PC-only profiler**. Also, the bridge executes past HALTCNT while halted in the synthetic PR #5 probe, but whether that defect explains this gameplay remains unknown.

## New screenshot-only indicators

Only when the game compiles **both** gbarecomp_runtime and gbarecomp_armv4t with GBARECOMP_PERF_HUD:

- **SWI 1/2/3:** top completed unsupported BIOS services by inclusive elapsed host time, **grouped by decoded SWI number**, with the most expensive guest **callsite** (ARM/Thumb), aggregated calls and interpreted instructions. ARM immediate decodes bits 23:16; Thumb uses the low byte, recorded before the SVC-mode switch and only after HLE declines the service. The callsite is the original instruction address (LR − 4 ARM; LR − 2 Thumb). Per-callsite accounting is done at bridge completion, not per interpreted step.
- **PC 1/2/3:** most frequently sampled guest instruction addresses *inside all interpreter bridges*, not necessarily only the most expensive SWI. Sample approximately 1 out of 128 interpreted instructions in a bounded 4096-slot hash table with up to four probes. Samples are approximate **frequency indicators, not instruction counts or precise per-PC time**. HASH DROPS reports collisions/overflow. No clock or map lookup is performed per interpreted instruction.
- **BRIDGE IRQ:** wall-clock time spent processing the outermost IRQ handler call while an interpreter bridge is active. It is already **included** in BRIDGE/GUEST; do not add or subtract it mechanically. Nested IRQs within the same call are not double counted by the IRQ timer.
- **HALT SAMPLES:** the number of internal-PC samples for which GbaIo::halted() was true after an interpreted instruction. This is **not** a count of HALT events or proof of an incorrect wake condition. Zero is not conclusive because it is sampled.
- **ACTIVE PC:** bridge entry address being interpreted at the frame sampling point, not the exact internal instruction PC. Long-running bridges may not yet appear in the completed SWI rankings.
- Existing host FPS, guest speed, PPU scanline, guest and presentation times remain available.

No CSV, ADB, root, log or Android/data access is required. The user installs one manually built ARM64 APK of the game's matching diagnostic branch and takes a landscape screenshot during the first stage after ~2 seconds of gameplay.

## Important limits

All SWI bridge timings are **inclusive** of runtime_tick, IRQ processing, PPU and any nested guest work during the bridge. SWI totals may overlap if nested SWIs invoke nested bridges. The "most expensive callsite" named for a given service is just that site's largest completed inclusive-time contribution, **not** necessarily the caller of every execution of that service. PC samples are over all bridges and are **not attached to individual SWIs**. If all calls are short and numerous, sampling every 128 steps may miss some internal PCs; the sampling rate and HASH DROPS must be considered.

The HUD changes host-side work and can reduce diagnostic APK FPS. It makes **no performance improvement claim**. It does not edit the HALT implementation or generated GBA/BIOS sources. Original ROM, BIOS, save states and emulated cycle accounting are unchanged.

## Delivery and validation

- Game branch: `diag/bios-swi-live-attribution`. Enable GBARECOMP_PERF_HUD on **both** `gbarecomp_runtime` and `gbarecomp_armv4t` (the latter records SWI number/callsite and measures IRQ host time).
- Engine branch: `diag/bios-swi-live-attribution` based on PR #3. No changes to PRs #1–#5 or main.
- Pin the engine revision in the game's Codemagic YAML and the manual GitHub Actions workflow.
- Source review and isolated C++20 syntax checks were performed; **no complete ARM64 APK or end-to-end runtime test has been executed** in this environment. The result must be tested in one manual Codemagic build.
- After a screenshot identifies the dominant SWI service and PC sites, compare those precise paths to BIOS HLE/LLE and PR #5's HALT reproduction before choosing an implementation. Do **not** infer that SWI 04/05 is the root cause without runtime evidence.
