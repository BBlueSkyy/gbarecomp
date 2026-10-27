# Owned IRQ/SWI continuations in the common runner

Base: `fix/bridge-swi-return-boundary`, engine
`3886062fad7574a1aea03ed60e5f6b63dfac79cb`.
Reference: [PR #8](https://github.com/BBlueSkyy/gbarecomp/pull/8),
`d0abe6a5a96294f4248e16a6e610719a5957b439`.

This change establishes ownership and persistence for the common runner. It
does **not** suspend at HALTCNT, implement VBlankIntrWait HLE, change wake
eligibility/latency, or claim an FPS improvement. HALT remains the next stage.

## Ownership and execution contract

`RuntimeIrqContinuation` remains the shared representation: preserved R0–R3/R12,
caller return floor/depth and previous iret sentinel. IRQ entry now records
these values in the active owner's `interrupts` vector for both runner policies.
The unbound legacy ABI retains a fallback vector for standalone callers/tests.

The common runner binds a local `RuntimeArmContext` for its whole lifetime,
then unbinds it before that object is destroyed. The live CPU/window remains
`g_cpu` and the existing globals; the IRQ vector is owned immediately, and
capture copies the complete window, BL returns and SWI ledger by value.

Two bindings deliberately retain different scheduling policies:

- `runtime_set_owned_context()` owns values but continues to drive a newly
  entered IRQ synchronously. Normal instruction/IRQ completion order is retained.
- `runtime_set_resumable_context()` keeps the existing multiplayer asynchronous
  dispatch/yield policy. Owning a common context does not enable multiplayer's
  strict native-only coverage requirement or its scheduler.

**Bind the destination before restoring a different context.** Restore updates
the live window and the bound owner's IRQ vector. Multiplayer's bind sites and
tests follow this order; its execution scope restores the previous common owner
before restoring that owner's captured values. Callback/filter wiring is trusted
host state. Context binding is legal outside native dispatch.

`runtime_resume_irq()` factors the existing synchronous drive-to-iret loop.
It drives exactly the depth present at entry and restores that owned frame's
values after the return instruction's tick, as the old synchronous path did.
It does not retain a vector-element reference across nested dispatch. A restored
outer IRQ is driven separately by the runner after the restored inner IRQ ends.
If a valid saved boundary already has `iret_depth == irq_depth`, the driver
completes metadata without executing another guest instruction or adding cycles.

The runner's dispatch boundary catches `RuntimeDispatchYield`, after C++ frames
have unwound, and marks surviving guest returns as hostless through the existing
stack-restore mechanism. This includes IRQ delivery during idle pumping. No new
HALT yield site is added here. The existing HALT check/pump in the outer runner
is unchanged.

The SWI ledger still validates returns from SVC. The bridge also recognizes a
return that consumes its entry ledger level when entered at an internal BIOS PC,
including a System-mode service continuation. Explicit forced-stop validation
retains its old subtree contract; IRQ iret signaling remains unchanged.

## Snapshot v5 and compatibility

The CPU0 payload is unchanged: CPU and banked registers, BL stack, SWI ledger.
A required `EXEC` section, schema 1, stores only explicit little-endian values:

- Return floor, IRQ depth/iret sentinel, resume PC, HALT-origin indicator and
  IRQ-entry counter.
- IRQ continuation count and each record's five registers/floor/depth/sentinel.
- The existing `RuntimeTimingContext`: clock, VBlank/yield counters, pending
  device-cycle debt and next-event budget.

No callback, owner pointer, bus/PPU pointer, C++ frame address or host function
address is serialized. Load retains the current executable's trusted hooks and
the current owner binding. It does not restore wiring from a previous process.

Persisting lazy timing debt is necessary for comparing execution after load:
the devices may be behind the CPU by a few cycles at a valid boundary. Capture
and restore do not tick devices or charge new cycles. EXEC is authoritative over
advisory META, including when META's clock pointer aliases `g_runtime_cycles`.
The common runner's established diagnostic cycle re-origin after file/rewind
load is retained. Pending debt can therefore exceed that re-originated clock;
the loader does not reject this valid state or discard its debt.

Version 4 is the previous supported layout. Its CPU/BL/SWI payload and devices
load as before. Its clean-boundary contract supplied no active IRQ residue, so
migration creates an empty IRQ stack/floor/sentinel and clears unavailable lazy
debt. META's clock is retained when present; a zero horizon re-arms at the next
normal tick. **Exact historical pending debt cannot be recovered from v4**, which
never stored it. Migration does not invent an IRQ continuation from CPSR.mode.
Previously unsupported v1–v3 layouts remain rejected. A v4 file carrying EXEC
is rejected, rather than guessing which schema to trust.

Preflight rejects excessive counts, inconsistent depth/floor chains, saved
iret sentinels beyond their own parent level, invalid HALT-origin values,
unsupported EXEC schema, invalid timing bounds, missing EXEC and unsupported
versions before committing CPU/context/device state. Existing MODS identity
preflight remains in force.

Snapshots must be taken at a valid execution boundary. Save/load while a
synchronous IRQ entry/driver is still active is rejected: a host service hook inside
`runtime_tick()` can see post-instruction CPU before that instruction's cycle
cost has been charged. Capturing clock fields cannot make that an instruction
boundary. The completed-iret tests construct a clean boundary **after** charging
the instruction, rather than endorsing mid-tick saves.

## Validation

`continuation_tests` reuses the PR #8 experiment as assertions of the corrected
contract. Before implementation, its first two assertions failed against the
base libraries: different preserved IRQ registers had identical blobs, and a
fresh owner lost the IRQ continuation. With v5, the single-record test produces
distinct 553,560-byte blobs and restores IRQ depth/floor/continuation count 1.

Additional acceptance covers:

- Fourteen ARM/Thumb pairs: IRQ, SWI within IRQ, nested IRQ, SWI within nested
  IRQ, mainline SWI, nested SWI and an internal BIOS System-mode continuation.
  Synchronous execution is compared with an explicit authored yield, save,
  rebinding to another owner, load and resumed execution. Complete snapshots
  match immediately after load and at the final common execution boundary.
- Registers including banked values/SPSRs, master cycles and pending debt, BL
  limits, SWI ledger, full bus/IO/audio/PPU/save serialization and resumed native
  caller execution exactly once.
- A fresh machine after the original bus, PPU and owner are destroyed; current
  host hook wiring survives and the full final state matches the reference.
- One and two already-completed iret levels: no extra dispatch or cycles.
- Malformed continuation/timing metadata and version/schema failures leave
  the entire prior machine snapshot unchanged; META mismatch/absence cannot
  override the v5 execution clock.
- A genuine v4 writer fixture, generated using the base engine's libraries,
  loads CPU, BL and SWI state and actually completes its internal SVC return.
- Nonzero serialized prefetch cursor and a re-originated clock with pending
  debt survive roundtrip. No bus/device implementation changes are needed.
- An atomic IRQ-entry read callback and an active handler cannot save partial
  execution; their requests are rejected without changing the output buffer.
- The PR #8 masked-wake observations remain unchanged. They are not a new wake
  implementation or a hardware timing qualification.

All code and state inputs are authored, ROM/BIOS-free fixtures. The native test
entries and short interpreted exception returns are **not** implementations of
Div, VBlankIntrWait or the proprietary BIOS. Comparisons qualify continuation
transport/execution, not full BIOS-service or Android gameplay parity.

Build and run locally:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGBARECOMP_BUILD_ORACLE=OFF
cmake --build build --target continuation_tests bridge_iret_tests swi_snapshot_tests \
  bridge_swi_return_tests irq_pending_tests multiplayer_session_tests bus_tests -j2
ctest --test-dir build --output-on-failure -R \
  '^(continuation_tests|bridge_iret_tests|swi_snapshot_tests|bridge_swi_return_tests|irq_pending_tests|multiplayer_session_tests|bus_tests)$'
```

These seven focused tests passed on x86_64/GCC 13.3. A clean GCC build encounters
the preexisting conditional-lambda/nullptr error in `runtime.cpp`. Local builds
used the same temporary `+[]` conversion as previous investigations, removed
from the published diff. No application optimization flags were changed.
The standalone PR #5 HALT probe still exposes the old HALT defect; fixing that
defect is deliberately excluded. There is no performance benchmark or claimed
device speedup in this stage, and no Android/NDK build was run.

The change leaves the game's engine pin, Android port, Codemagic, all 16 `-O1`
shards, previous PRs and `main` untouched. No remote build or merge was requested.

## v4 fixture provenance

`tests/runtime/fixtures/make_v4_fixture.cpp` must be compiled with the base
engine's v4 headers/libraries (a static_assert prevents accidental v5 output).
It writes only authored register/RAM state, an empty cartridge and an all-zero
test ROM identity. The RLE header expands to 553,456 bytes, SHA-256
`e1b64be1b0f553dc1359a974f397a4df662978c365fdefca4382250b07e78afb`.
RLE is test-data compression, not a new snapshot format. No game or BIOS binary
is included in the fixture.
