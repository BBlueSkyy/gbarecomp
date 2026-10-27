# SWI fallback: bound interpretation at exception return

## Finding and scope

Base: `3dc84528e741157a14bc1e901f347fb3f9dcf3d0` (`perf/runtime-memory-hotpaths`, engine Draft #1). This change preserves that branch, its PR and main.

A reproducible interpreter amplification exists when a generated caller executes a SWI while the guest BL-return stack is nonempty and the BIOS vector has no native coverage. `runtime_swi()` enters Supervisor mode and dispatches vector 8 without opening a guest BL frame. The miss bridge nevertheless selected the existing BL frame's return PC as its stop. After the BIOS's SVC exception return it interpreted the caller's remaining subtree, including code that already had a native entry, until that caller returned.

The ordinary missed-subroutine contract is intentional and remains unchanged. The SWI exception itself does not own that caller BL frame. Its bridge can stop after its interpreted SVC exception return, preserve the frame, and let the existing generated cancellation/redispatch protocol resume native code at the post-SWI continuation.

The production change is limited to the unforced vector-8 bridge entered in SVC mode. Its new break runs **after** interpreter SPSR/bank restoration, SWI-ledger validation and the instruction's existing runtime_tick. It skips only the erroneous consumption of the interrupted caller's BL frame. Forced-stop validation, ordinary missed subtrees, IRQ-mode exception returns, byte verification, hooks, dispatch tables, device stepping and cycle values are not changed. No generated sources, private BIOS, ROM or HLE replacement are required.

This is a proven runtime path and exact host-work reduction. It is **not proof that this path predominates in Dragon Ball gameplay**, or that its slowdown is fixed on Snapdragon 870.

## Relation to the Android configuration

The reviewed Android workflows fetch the pinned engine and UI, generate ROM-free configuration and compile the existing game shards. They do not generate BIOS AOT bodies. The engine selects `bios_dispatch_stub.cpp` when `bios_recompiled.cpp` is absent, with BIOS dispatch length zero. The game configuration does not opt into BIOS HLE. Android process setup defaults `GBARECOMP_SELFHEAL_RECOMPILE=0`, so missing coverage cannot automatically become native through on-device compilation; an already supplied/cached overlay is a separate case.

Thus the SWI miss route is relevant to this configuration, without establishing how many times gameplay takes it with an open BL frame. Mere BIOS absence alone does not quantify performance.

## ROM-free reproduction and validation

All fixture instructions were authored for the test, not extracted from a BIOS or game. An initial minimal reproduction uses a synthetic vector-8 `MOVS PC,LR`, an open caller BL frame, and a native-registered continuation containing `ADD`/`BX LR`:

| Return from runtime_swi | Baseline | Fixed |
| --- | --- | --- |
| PC | caller's outer return | post-SWI continuation |
| Caller ADD already interpreted | yes | no |
| Caller BL frame | consumed | preserved for native unwind |
| Interpreted instructions | 3 | 1 |

`bridge_swi_return_tests` compares the native translation of that authored BIOS instruction against the real miss/bridge route for ARM and Thumb, guest call depths 0/1/3, live-IRQ continuation mode, and generated cancellation/redispatch. It checks CPU/banked registers, guest cycles, call depth, SWI ledger, IO hash, audio sample count, PPU phase and scheduler horizon. Additional cases exercise nested SWIs, explicit forced-stop passes and ordinary missed subtrees. A pending-IRQ case actually invokes `runtime_irq`, acknowledges IF and checks protection of a live native call frame at floor 1.

The regression fails against #1 and passes with this patch. Ten focused CTests passed: `bridge_swi_return_tests`, `bridge_iret_tests`, `swi_snapshot_tests`, `tail_dispatch_tests`, `bus_tests`, `timer_tests`, `dma_tests`, `irq_pending_tests`, `ppu_smoke_tests`, `audio_playback_tests`. An independent reviewer reran the regression and both workload modes. These are focused fixtures, not broad hardware or game acceptance.

### Amplification workload

The opt-in `swi_bridge_amplification` target starts the same missing BIOS SWI and then executes a 10,000-iteration ARM caller loop (`ADD`, `SUBS`, `BNE`, then `BX LR`). Both binaries link the actual runtime. The existing `self_heal_interpreted_insns()` counter measures the absorbed subtree; no production instrumentation was added.

| Mode | Interpreted before | Interpreted after | Guest cycles, both | FNV-1a state hash, both |
| --- | ---: | ---: | ---: | --- |
| Ordinary SWI | 30,002 | 1 | 50,007 | `05a1ba3b57a5fce1` |
| SWI with pending IRQ | 30,006 | 5 | 50,016 | `9f547cc01820372e` |

Each matching 545,066-byte serialization includes bus/IO/audio/PPU/CPU and timing values. The caller runs once natively in the candidate; the baseline interprets it. The four remaining interpreted IRQ-wrapper instructions are preserved. The fixture pins its host RTC for comparison only and disables frame yielding only inside its standalone process; no application setting changes.

On Linux x86_64, GCC 13.3.0, O3 runtime libraries and O1 fixture, 12 alternating launch pairs per mode on one permitted CPU measured first-call medians of 2.723 ms → 0.553 ms (ordinary) and 2.490 ms → 0.903 ms (pending IRQ). The measurement includes first-miss bookkeeping in both variants and is host-sensitive. Raw output is in `tests/performance/results/2026-10-26-swi-boundary-host.json`. **These are synthetic host timings, not Android or gameplay gains.** No compiler optimization flags changed in the application.

### Reproduction commands

Build the exact same fixture source against the baseline and candidate runtime stacks. Its target is excluded from ordinary builds and does not run automatically:

```sh
cmake -S <candidate-engine> -B <build> -DCMAKE_BUILD_TYPE=Release -DGBARECOMP_ENABLE_MODS=OFF
cmake --build <build> --target bridge_swi_return_tests swi_bridge_amplification
python3 tests/performance/compare_swi_bridge.py <baseline-fixture> <candidate-fixture> --pairs 12 --output comparison.json
```

To build the baseline fixture, register/copy only the test target and sources against the #1 runtime; do not copy the production fix. Existing GCC 13 lambda/nullptr compilation incompatibility in runtime.cpp:1921 required the same local unary-plus conversion as the previous investigation. That unrelated workaround is excluded from publication.

## Boundaries and remaining uncertainty

- The effective game's miss PCs, instruction counts per frame, RAM-copy destinations, registered template catalogue and byte-verification outcomes are not established by these fixtures. Large generated-file contents could not be retrieved through the repository file interface; no regeneration was attempted.
- `lookup_native_ram()` validates the entire candidate byte sequence (including cached hits), checks instruction mode, and rejects modified bytes before selecting AOT. Registration is emitted in the generated dispatch translation unit's constructor. Rebinding, mutable-code guards and fallback remain intact; no game-specific RAM template or address was invented.
- A RAM miss can still interpret its ordinary entire subtree. Without the actual executing addresses and matching source bytes, its contribution and any valid discovery fix remain uncertain.
- Mod callbacks which author a return keep their existing whole-subtree callback ABI. This patch addresses interpreted SVC exception returns, not every callback-authored return path.
- The pending-IRQ fixture exposed a **preexisting** distinction: the interpreter restores SPSR before ticking an SVC return, whereas generated native BIOS code ticks first. The patch does not move either tick. The IRQ fixture asserts the bridge's existing saved-PC/CPSR behavior, and baseline/fixed IRQ workload state matches. It does not claim to fix native/interpreter timing parity or qualify every nested hardware-IRQ combination.
- PPU, audio and scheduler are not modified. No new evidence identifies them as the predominant device bottleneck. No additional unrelated micro-optimizations were added.

Best next validation within the requested constraints is an ordinary gameplay test of a manually built APK pinned to this engine revision, with no new log/ADB/CSV procedure. The repository-only evidence cannot establish the fraction of gameplay time spent here. The change must remain Draft and must not be presented as a confirmed fix for Dragon Ball's overall speed.
