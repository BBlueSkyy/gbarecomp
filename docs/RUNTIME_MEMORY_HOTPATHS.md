# Exact runtime memory hot paths (2026-10-26)

This is a host-cost optimization, not a fix proven to eliminate an Android game's slowdown. No ROM, BIOS, Android device or APK was used in these measurements. Generated guest code, cycle values, IRQ delivery, scheduler horizons, frame pacing and device implementations remain unchanged.

## Measured cost and change

The generated instruction prologue/epilogue calls runtime_should_yield and runtime_tick. A guest load additionally enters the bus and asks for data-access cycles. Those operations crossed several small separately compiled functions even with an optimized runtime:

- Each ordinary bus read/write called classify and resolve_offset in a different translation unit. Their exact bodies now live inline in gba_memory.h, allowing the compiler to fold the region and offset selection into each bus operation. No direct RAM bypass was added: MMIO, mirroring, ROM mapper, EEPROM/flash/GPIO, open bus, observers and read overrides still use the same bus code.
- runtime_tick called GbaIo::irq_pending for every instruction. The same IE/IF/IME-bit-zero query is now inline, removing that call without caching pending IRQ state or reducing the frequency of checks.
- The concrete GbaBus::access_cycles override now shares the pure inline rwt_access_cycles implementation with waits_access_cycles. This removes only the inner wrapper. runtime_mem_cycles still invokes the **virtual** bus method, preserves width narrowing and null-bus behavior, and honors derived bus overrides. An initial direct-table experiment failed the new subclass test and was corrected before publication.

The dispatch cache already has 4096 validated entries. This change does not replace dispatch resolution, its hooks, resume handling, mutable-code guards or interpreter/overlay fallback.

## Host benchmark

Source: tests/performance/runtime_bench.cpp; comparison driver: tests/performance/compare_runtime.py. It links the actual engine libraries and uses a single synthetic native dispatch entry. The caller is O1; runtime libraries are GCC 13.3.0 Release O3 on Linux x86_64. Eight alternating A/B launch pairs, seven samples per launch, single permitted CPU affinity. Every scalar sample executes 1,000,000 operations; the mixed sample executes 200,000 batches, each containing three arithmetic boundaries, one load and one store.

Medians (ns per operation; mixed row is ns per five-instruction batch):

| Path | Baseline e353848 | Candidate | Cost reduction |
| --- | ---: | ---: | ---: |
| runtime_tick(1) | 7.8685 | 6.7825 | 13.8% |
| EWRAM read32 | 11.6625 | 8.5035 | 27.1% |
| data-cycle query | 5.2615 | 4.9935 | 5.1% |
| arithmetic instruction boundary | 14.4005 | 12.6030 | 12.5% |
| load instruction boundary | 32.3085 | 27.0045 | 16.4% |
| mixed five boundaries | 100.9830 | 85.5610 | 15.3% |

Raw samples and complete results, including yield and dispatch controls, are in tests/performance/results/2026-10-26-host.json. These are not FPS figures or a forecast for Snapdragon 870. Layout and host scheduling affect nanosecond microbenchmarks: earlier exploratory builds showed yield-only slowdowns, and the unmodified dispatch/yield paths varied between builds. No independent dispatch/yield speedup is claimed.

All 16 final launches produced the same 86,800,000 guest cycles, 309 PPU frame wraps and CPU result. The 545,042-byte combined serialized bus/IO/audio/PPU/CPU fixture had FNV-1a d998a3524f656302 in every launch. The fixture pins the otherwise host-derived RTC value so serialization is comparable. Its frame yield is disabled only inside the standalone benchmark; no player setting or shipping synchronization was changed. This does not test game-specific inputs or native coverage.

## Correctness and ARM64 checks

15 focused CTests passed: memory_timing_tests, irq_pending_tests, bus_tests, dma_tests, dma_bulk_tests, timer_tests, ppu_smoke_tests, stack_guard_tests, tail_dispatch_tests, diagnostic_capture_tests_disabled, diagnostic_capture_tests_enabled, audio_playback_tests, save_config_tests, pacer_tests and interpreter_smoke.

The new timing fixture makes 47,775,744 comparisons over WAITCNT configurations, memory-control changes (including rejected zero-wait settings), memory regions, width truncation, non-boolean sequential arguments, bus rebinding and unmodified prefetch state. It also tests a derived bus's virtual timing override and null bus. The IRQ fixture tests all 14 IRQ sources, matching/nonmatching enable bits, IME variants, non-destructive repeated queries and write-one-to-clear acknowledgement. Existing bus/DMA/timer/PPU/audio tests cover side effects beyond the pure helpers. This is focused coverage; no full-game acceptance is claimed.

GCC 13 rejects an existing baseline lambda/nullptr conditional in runtime.cpp:1921. Both host runtime builds used the same local unary-plus conversion on that lambda solely to compile the baseline. That unrelated line is not in this change.

Cross-compilation with Zig 0.17.0's Clang to aarch64-linux-musl at O2/PIC succeeded for baseline and candidate runtime_bus_bridge.cpp and gba_bus.cpp. Assembly showed six classify calls, six resolve_offset calls and one waits_access_cycles tail branch in the baseline bus translation unit and none in the candidate. Both runtime instruction IRQ-query calls were removed. runtime_mem_cycles retains its indirect virtual branch. This is ARM64 code-generation evidence, **not** an Android NDK build/link, on-device benchmark or Bionic/TLS qualification.

## Reproduction

Build both revisions with the same compiler/options and the exact benchmark source from this change. The benchmark target is excluded from ordinary builds; it is never run automatically and needs no ROM:

```sh
cmake -S <engine> -B <build> -DCMAKE_BUILD_TYPE=Release -DGBARECOMP_ENABLE_MODS=OFF
cmake --build <build> --target runtime_hotpath_bench memory_timing_tests irq_pending_tests
python3 tests/performance/compare_runtime.py <baseline-binary> <candidate-binary> --output comparison.json
```

For the unmodified baseline, copy the benchmark source and register the same opt-in target, with O1/g0 compile options, against its libraries. On GCC 13 apply the same local lambda compatibility conversion to both checkouts. Use a clean diagnostic environment. The numerical measurements are evidence, not a CI pass/fail performance threshold.
