# Bridge HALT suspension implementation plan

> Execute inline with test-driven-development and executing-plans; independently review the final branch.

**Goal:** Stop guest instruction execution during HALT and resume the exact owned IRQ/SWI continuation.
**Architecture:** Use RuntimeDispatchYield at instruction boundaries, preserving the PR9 value-owned execution context. Idle time continues through the common runner's existing event-driven pump and runtime_tick; wake eligibility is distinct from IRQ delivery.
**Tech Stack:** C++20, CMake/CTest, authored ARM/Thumb fixtures with no ROM/BIOS.
**Spec:** User request for PR10; base 66cb8925519c078de38af543b81a691a350f969a; PR5/8 reports and docs/COMMON_RUNNER_CONTINUATIONS.md.

## Global constraints

- No HLE, interpreter rewrite, artificial cycle/IRQ/flag changes, private binaries, Android/shard/workflow changes, remote builds or merge.
- Preserve snapshots v5/v4 migration and all execution metadata from PR9.
- Publish a draft only after semantic qualification; otherwise publish investigation without unsafe production changes.

## Review focus

- HALT inside a synchronously delivered IRQ must unwind without driving a halted instruction or dropping its IRQ frame.
- A waking nested IRQ must finish before the suspended outer IRQ/SWI resumes.
- A HALT boundary must preserve native BL frames; a service return must consume only its own SWI ledger entry.
- Bounded self-heal validation must reject an unfinished HALT pass without normal return cleanup.
- Lazy device timing debt must not shift the next timer/PPU/audio event during idle pumping or restore.

### Task 1: Reproduce instruction and wake contracts

Files: tests/runtime/halt_resume_tests.cpp; CMakeLists.txt.
- [x] Reuse authored PR5 HALTCNT vector and PR8/9 continuation fixtures. Assert PC14/R2=0/HALT/cycles9, no fetch/instruction while halted, preserved BL/SWI.
- [x] Assert no wake for IE&IF=0; for pending enabled sources wake regardless of IME/CPSR.I and deliver only when unmasked. Retain the established 7-cycle common-backend wake window, documenting the reference/frontend divergence instead of importing multiplayer's 4-cycle constant.
- [x] Run against unchanged PR9 and record expected failures before implementation.

### Task 2: Suspend and resume through owned common execution

Files: src/runtime/runtime_arm_default_aborts.cpp; src/runtime/runtime_bus_bridge.cpp; src/runtime/runtime.cpp; runtime context only if needed.
- [x] Add pre-fetch HALT suspension without returned-frame cleanup; bounded bridge passes return failure rather than treating HALT as completion.
- [x] Ensure common native/forced-interpreter HALT and IRQ drivers use the same exception unwind boundary. Clear dispatch suspension after unwind and before idle/IRQ delivery.
- [x] Separate wake condition IE&IF from delivery masks, retaining existing device ordering and wake-cycle constant.
- [x] Reuse the common idle pump; test current pending debt vs event horizon, future/pending wake, nested IRQ/SWI, internal BIOS PCs and BL limits.
- [x] Test repeated authored SWI05 waiting, no-save vs fresh-owner save/load full blobs and clocks/device state, both ARM/Thumb; reject incomplete acceptance.

### Task 3: Qualify and publish

Files: docs/BRIDGE_HALT_RESUME.md; focused test targets above and existing continuation_tests, bridge_iret_tests, swi_snapshot_tests, bridge_swi_return_tests, irq_pending_tests, multiplayer_session_tests, bus_tests, timing/timer/DMA tests.
- [x] Run relevant regression suite and a full configured CTest run where viable; explicitly report unavailable tests/preexisting compiler blockers.
- [x] Compare the PR5 reference boundary and run full-path authored workload; distinguish eliminated wrong instructions from synthetic host cost and actual Android FPS.
- [ ] Fresh independent review, address correctness blockers, verify clean diff/base, commit and publish Draft engine PR10. Game pin only if qualified and useful for future Android build; never trigger builds.

## Execution ledger

- Isolated worktree halt-resume, branch fix/bridge-halt-resume; exact base verified.
- Reused PR8 investigation and PR9 report. No repeated attribution investigation.
- Wake semantics: GBATEK says IE&IF wakes independently of delivery masks. mGBA gba.c schedules a 7-cycle event on IE&IF and clears HALT before testing IME/CPSR.I. Existing bios_smoke has a masked-wake bug and distinct already-pending behavior; compare/report, do not silently claim hardware equivalence or copy multiplayer's 4-cycle policy.

- Task 1 complete: red.log records 11 expected failures against PR9.
- Task 2 complete: halted fetch/return guards, owned native unwind, masked wake split and shared event-horizon pump. idle-red.log independently reproduced extra pending-source delay.
- Task 3 validation: initial full run 66/68; codegen invalid-floor fixture failure reproduced against base libraries; supplied a matching IRQ record, retaining production validation. Restored local executable bit on swi_snapshot_tests. Final full run 68/68 in 3.06s.
- Candidate/reference pairs compare materialized final states; no-save/load pairs additionally compare raw lazy debt before materialization. Fresh-machine HALT restore passes.
- Synthetic benchmark: five alternating runs recorded in experiments/halt-resume/bench-results.json. 210166 old halted instruction fetches versus zero; timing/IRQ sequence differs, no equivalent-execution speedup or Android claim.
- Compiler workaround +[] removed from publication after building, as documented.

- Final review identified wake-window DMA debt. New HBlank-DMA masked-wake case failed twice; reused drain_dma_steal after wake tick and before IRQ delivery. Final focused validation 11/11 (0.51s); full 68-test run not repeated after this final fix, per user's request.
- User steering: do not repeat prior investigations/reviews/tests; finish publication after essential checks.
