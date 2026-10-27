# Android live performance HUD (diagnostic only)

This branch isolates diagnostics from the shipping game. Enable `GBARECOMP_PERF_HUD=1` on the `gbarecomp_runtime` target in the Android game's CMakeLists.txt. No ROM or BIOS is embedded or required by this change.

## Android owner workflow

Use the game's `diag/android-live-performance-hud` branch and its matching engine commit in Codemagic. Start **one manual ARM64 Android build**, install the APK, enter the *first playable Goku stage*, and take a **landscape screenshot of the live overlay** while the game is slow. You do **not** need ADB, an external log, CSV, root, the Android/data folder, or a game quit. Never rely on a window-title FPS readout on mobile.

HUD rows refresh roughly every 1 second:

- **FPS**: presented host frames per wall second, not guest instructions.
- **GBA SPEED**: guest master-clock cycles per wall second, divided by 16,777,216. 100% approximates full-speed GBA; excessive or low values need examination with the other fields.
- **GUEST ms**: wall time between host frame work, **inclusive** of CPU/interpretation, device ticks, PPU scanlines, in-bridge work, and guest-side game hooks.
- **HOST VIDEO ms**: end-of-frame raster copy/render, SDL upload/draw/present. Excludes scanline rendering already charged to GUEST.
- **BRIDGE DONE ms**: duration of **completed** interpreter bridges divided by presented frames; nested bridges do not double count. Calls/s counts bridge entries.
- **ACTIVE** suffix: at overlay sampling time, an interpreted bridge is ongoing, so its current duration is **not yet** in BRIDGE DONE. Do not mistake 0 completed milliseconds for 0 active interpretation.
- **BRIDGED instr/s**: live instructions executed through the bridge loop, including ongoing bridges. An opt-in increment per interpreted instruction is the only per-step HUD cost. Does not include explicitly forced whole-program interpreter mode; that mode is not selected for the game.
- **RAM BRIDGE/s**: bridge calls whose entry PC lies in EWRAM (02) or IWRAM (03). A zero value does not prove RAM AOT coverage is complete (native RAM templates are a separate path).
- **PPU SCAN ms/frame**: actual scanline-compositor wall duration summed for the period. It is already part of GUEST and can overlap BRIDGE; do not add these numbers.
- **AUDIO, INPUT, WAIT ms**: host finalization/audio queue, input pumping, frame pacing; diagnostic buckets from the existing frame-phase timer.
- **DISTINCT DISPATCH MISSES**: session-wide unique PCs. This is cumulative, **not** a per-second count.

The panel uses the existing `HostOverlay`/ImGui backend in the Android game window. It is drawn as host chrome after the game texture, without modifying the guest framebuffer or serialized GBA state. The overlay hides when the settings menu is open. Existing host overlays are chained. With no runtime UI font backend, `text_supported()` can be false, in which case the panel deliberately does not draw.

The old `GBARECOMP_FRAME_PHASE` CSV remains explicitly opt-in; the HUD works when it is absent and writes no report file.

## Caveats and review boundaries

Host-clock reads (~160 scanlines/frame plus frame boundaries), ImGui text each presented frame, and a bridge instruction increment have measurable overhead. Use this APK for **classification of bottlenecks**, not a precise shipping FPS comparison. An update at a frame boundary cannot appear if the guest never reaches/presents a frame. The interpretation timer records only completed bridges; the live counter/ACTIVE marker distinguishes ongoing bridges. Nested guest device timing may overlap the bridge bucket.

No Android APK was compiled or installed here; this is an opt-in, unmerged Draft pending one owner-run manual Codemagic build and visual confirmation. Do not merge it with the game/performance PRs or claim a gameplay speedup.
