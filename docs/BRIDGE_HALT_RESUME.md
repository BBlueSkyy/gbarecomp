# HALT suspension and continuation in the common runner

Base: engine PR9, `feat/common-runner-continuations`, exactly
`66cb8925519c078de38af543b81a691a350f969a`. References:
[PR5](https://github.com/BBlueSkyy/gbarecomp/pull/5),
[PR8](https://github.com/BBlueSkyy/gbarecomp/pull/8) and
[PR9](https://github.com/BBlueSkyy/gbarecomp/pull/9).

This fixes the reproduced execution-after-HALTCNT defect. It is not a
VBlankIntrWait HLE implementation or evidence that Android gameplay now reaches
full speed. No game ROM, BIOS image, generated shard or private binary is used.

## Defect and execution contract

The bridge previously executed the HALTCNT store and its instruction tick,
then fetched/ran the next instruction even while HALT remained active. Its
normal teardown could also consume a native BL frame as though the interrupted
service had returned. PR9 supplied ownership for the IRQ/SWI residue; this
change uses it rather than keeping host stack frames alive while waiting.

At a halted instruction boundary the bridge preserves the post-store PC and
CPU state, latches the existing BIOS prefetch convention, and calls
`runtime_suspend_dispatch()`. HALT takes precedence over coincident subtree
stop addresses, native entry points and exception-return completion. An
unbounded pass uses the dispatch yield protocol; a bounded validation pass
returns 0 (unfinished) without executing the next instruction or performing
returned-frame cleanup.

The common runner uses the exception form of `RuntimeDispatchYield`: ordinary
C++ unwinding leaves guest BL/SWI/IRQ continuations owned by the PR9 context.
Its catch boundary marks BL entries hostless and clears suspension before idle
pumping or delivery of another IRQ. A native/forced-interpreter HALT prologue
also uses the owned suspension path, so the synchronous IRQ driver cannot
mistake HALT for a completed handler or spin re-dispatching it. The temporary
forced-interpreter prologue marker is cleared even on an exception unwind.

After wake the existing runner resumes an owned IRQ first, or dispatches the
saved PC. PR9's return-floor, iret sentinel, banked register and SWI ledger
contracts remain unchanged. A service paused at an internal System-mode BIOS PC
still consumes its SVC ledger entry only at the real SVC exception return;
nested services and IRQs complete in their original order. No extra CPU
instruction, fabricated BIOS flag or IF acknowledgment substitutes for waiting.

## Idle advancement and wake versus delivery

The existing `pump_idle()` wrapper now delegates one chunk to the shared
`runtime_pump_idle()` implementation. It uses the live lazy event horizon,
which already accounts for charged device-cycle debt and includes PPU, audio,
timers and serial events. Device work and DMA cycle steals still go through
`runtime_tick()`. The runner keeps its existing frame-sized idle budget.

An already-pending enabled source is serviced at the idle boundary, without a
fictitious polling cycle or waiting for the next audio/PPU event. The helper
returns requested idle cycles, while wake and IRQ instruction costs are still
charged directly to the master clock. A zero restored horizon is re-armed by
the ordinary one-cycle scheduler tick. There are no host sleeps, busy CPU loops,
skipped device events or jumps directly to VBlank.

Wake eligibility is `IE & IF != 0`. Delivery additionally requires IME and a
clear CPSR.I. The common backend's existing **seven-cycle wake window is
retained**, with all devices advanced during that window even for masked wake.
Only interpreted/authored guest IRQ code acknowledges IF or updates the
BIOS/game flag. Disabling IE still prevents wake; IME/CPSR.I do not.

This condition is documented by [GBATEK, HALTCNT/Halt](https://mgba-emu.github.io/gbatek/).
For comparison, [mGBA's `GBATestIRQ` / `_triggerIRQ`](https://github.com/mgba-emu/mgba/blob/master/src/gba/gba.c)
schedules a seven-cycle event on IE/IF and clears HALT before testing delivery
masks. This supports retaining the established policy; it is **not** a new
hardware measurement of every wake phase.

### Reference frontend differences

The local forced-interpreter frontend and the bridge share the runtime device
clock. At the original PR5 boundary they now agree exactly: PC `0x14`, R2=0,
HALT=1, 9 cycles. Authored wait execution also compares their complete final
machine states, not just a serializer return value.

The separate `tools/bios_smoke/main.cpp` frontend is not silently changed:

| Case at an already-halted boundary | Common runtime after this change | Existing bios_smoke frontend |
| --- | --- | --- |
| Future eligible PPU IRQ | Device event then existing 7-cycle wake window | Pumps to event then 7-cycle window |
| Eligible IRQ already pending | 7-cycle wake window, no polling instruction | Top-of-step IRQ check clears HALT/enters IRQ without that window |
| IE/IF match, IME=0 or CPSR.I=1 | Wake through 7-cycle window, no IRQ delivery | Remains halted; its loop/return conditions do not separate wake from delivery |
| No IE/IF match | Remains halted, devices advance | Remains halted |

The multiplayer scheduler retains its separate four-cycle wake policy. Neither
frontend nor multiplayer is rewritten here. Consequently this qualifies common
runner suspension and its retained latency, not cycle equivalence between all
three existing scheduling policies or new hardware timing accuracy.

## Snapshot ownership and compatibility

No snapshot layout change is needed beyond PR9's v5 EXEC section. HALT is
already in IO state; the exact CPU/BL/SWI/IRQ metadata and lazy timing debt are
already in CPU0/EXEC. Save/load happens after host unwind at the normal clean
boundary. Current trusted callbacks remain current executable wiring, never
serialized addresses. Active synchronous IRQ entry/driver snapshots remain
rejected as in PR9.

The new tests load into another owner and into an entirely new bus/PPU/owner
after the original machine is destroyed. They compare entire bytes at HALT,
then all final bytes including timing debt before materialization. For
bridge-versus-forced-frontend comparisons only, pending device time is
materialized on both sides without adding cycles: the frontends have different
existing MMIO flush boundaries. Complete CPU, clocks, devices and all remaining
metadata must then match. No-save versus load comparisons also match before
this materialization, so it cannot hide lost timing debt on restore.

PR9's genuine v4 migration fixture and malformed-v5 validation remain enabled.
No new pointers or continuation fields are added to the file format.

## Validation and reproduction

`halt_resume_tests` reuses PR5's authored vector and PR8/9 ownership fixtures:

- No pending IRQ: exact post-HALTCNT PC/cycles, no following ADD or return;
  repeated halted bridge entry performs no fetch, guest instruction or mutation.
- All 16 combinations of IE, IF, IME and CPSR.I; masked wake preserves IF and PC.
- HBlank DMA inside the wake window: both IME states with CPSR.I masked;
  stolen cycles must be charged before CPU resumption or snapshot boundaries.
- Future timer overflow and already-pending wake through the actual shared idle
  helper, including lazy timing debt and no extra poll cycle.
- Native HALT prologue, HALT at a bridge stop address, and unfinished bounded
  validation without returned-frame cleanup.
- Three repeated SWI05 calls in eight ARM/Thumb scenarios: plain service,
  service inside IRQ, service inside two nested IRQs, and two nested SWIs.
  The interpreted authored wait writes HALTCNT and tests a guest-owned VBlank
  flag. The interpreted IRQ acknowledges actual IF and ORs it into that flag.
  It is an equivalent wait workload, not the proprietary BIOS implementation.
- No-save, new-owner save/load, and reference execution; exact BL/floor/ledger/
  iret/CPU/banked/SPSR/cycle state plus complete serialized bus/PPU/audio/save
  state. A fresh machine also restores a halted nested IRQ/SWI and completes
  the native caller exactly once.

The original red test against PR9 exposed 11 failed assertions (instruction
execution, premature return and masked wake). A separate idle-boundary red test
exposed the unnecessary pending-source polling delay. Both pass after the fix. Independent review identified wake-window DMA debt;
a focused test failed twice before adding the existing DMA drain after the
wake-device tick, before IRQ delivery. All 11 essential continuation/IRQ/SWI/
timing/timer/DMA tests passed after that final change (0.51 s). The 68-test full
run above preceded this final DMA correction and was not repeated.

Local x86_64/GCC 13.3 Release build: **all 68 configured CTests passed**,
including `halt_resume_tests`, `continuation_tests`, `bridge_iret_tests`,
`swi_snapshot_tests`, `bridge_swi_return_tests`, `irq_pending_tests`,
`multiplayer_session_tests`, memory timing, PPU, timer and DMA tests.
A full-suite run also exposed an old codegen test constructing a nonzero IRQ
floor with no IRQ continuation, invalid under PR9. Linking that test against
unchanged PR9 reproduces the failure. Its fixture now includes the matching
owned IRQ record; no production validation was relaxed. A local binary lost
its executable permission and was chmod'ed before the successful full rerun.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGBARECOMP_BUILD_ORACLE=OFF
cmake --build build -j3
ctest --test-dir build --output-on-failure -j3 --timeout 60
./build/halt_resume_tests --benchmark
```

As in PR5/8/9, GCC encounters the preexisting lambda/nullptr conditional in
runtime.cpp. Local compilation used the temporary unary-plus `+[]` conversion;
it is removed from the published change. Application optimization flags are
unchanged. No Android/NDK or remote build was run.

### Full-path authored cost comparison

`--benchmark` runs 120 SWI05 waits, including BIOS interpretation, exception
entry/return, IRQ guest flag handling, the normal idle helper and native caller
completion. There are no snapshots or host clocks inside the measured loop
other than the outer measurement. Compile the same test source with
`-DGBARECOMP_HALT_BASELINE` against PR9's libraries to reproduce the old path;
that test-only macro supplies the old runner idle helper, absent from PR9's API.
Use the same compiler/optimization settings for both harnesses.

Five alternating runs, GCC Release libraries and -O3 test harness, x86_64:

| 120 waits | PR9 | Candidate |
| --- | ---: | ---: |
| Authored interpreted instructions fetched | 211,986 | 2,880 |
| Of those fetched while HALT remained active | 210,166 | 0 |
| Guest cycles | 33,904,671 | 33,623,795 |
| IRQ entries | 121 | 120 |
| Median host time | 56.78 ms | 44.28 ms |
| Host time range | 52.13–63.61 ms | 35.96–46.37 ms |

These are **synthetic full-path observations**. The old path also performs an
extra IRQ/frame and has different guest timing because it illegally executes
while halted; the two benchmark states are not equivalent. Therefore the host
numbers are not an isolated speedup of an otherwise identical execution, a
measured saving of all 16.7 ms/frame, or an Android FPS prediction. Semantic
qualification comes from the candidate/reference full-state comparisons above.
The Moto G100 slowdown remains unverified after this change.

The game pin can be updated separately for a future Android build. No shards,
-O1 settings, Android code, Codemagic configuration, previous PRs or main are
changed here. No merge, build dispatch, ADB/CSV/device collection is performed.
