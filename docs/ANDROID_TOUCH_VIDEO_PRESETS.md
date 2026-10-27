# Android touch & video presets (scoped feature branch)

This feature branch is based on `fix/bridge-swi-return-boundary` (PR #2), **not** the diagnostics HUD branch. It preserves the guest execution path, generated shards, IRQ/DMA and pacing. No claim of improved guest FPS.

## Touch
- Virtual D-pad recognizes diagonals with independent axis qualification (no longer snaps to just one axis); a dead zone and axis ratio reduce off-axis jitter.
- Control size presets 80%, 100% (default), 120% through the runtime **Controls** settings. The visual geometry and touch hitboxes use the same layout. The scale is clamped to available safe width so the D-pad and A/B do not overlap.
- Existing finger ownership, slides, Start/Select, L/R, touch-pad toggle, cutout insets, controller mapping and menu long-press are retained.
- Selected size persists as `[Touch] pad_scale` in the app's existing config.ini.

## Video
- `Display > GBA color profile`: Raw (default, exact passthrough), Backlit, Classic, Frontlit, Unlit; these reuse the existing present-only BGR555->RGB888 LUT. When changed the LUT is rebuilt once, and the frame buffer and snapshots stay raw.
- `Display > Scanline effect`: Off (default), Subtle, Strong. Android uses a cached **1-by-height** ABGR8888 transparent-black pattern with a **single additional SDL_RenderCopy GPU draw** over the game's image each frame. It does not rasterize scanlines on CPU, modify guest pixels, or shade the touch controls. Rebuild only on preset/height change.
- Presets are stored under `[VideoEffects]` in existing config.ini and restored on launch.

**Shader status:** The scanline overlay is an inexpensive *shader-like presentation effect*, **not** a custom GLSL shader. Arbitrary shader pipelines (CRT masks, bloom, LCD grid, xBRZ/SMAA) require a separate GPU backend implementation and additional tests on Adreno 650/Turnip; not included here. Avoid expensive postprocessing until gameplay bottleneck resolved.

## Validation and limits
Code inspected against the original HostWindow renderer, pinned recomp-ui choice initializer ABI, ROM-free pipeline and display/touch API. No NDK/Bionic APK compilation or on-device measurements performed. One owner-started **manual Codemagic** build is required after review; no automatic remote build, ADB, CSV or logs. Test D-pad diagonals, two-finger A+B, portrait and landscape layout, resize/insets, scanlines and color profile, save and reopen.

This is a functional UI/host-only enhancement, separate from the interpreter-performance and HUD diagnostic PRs. Do not merge until device validation.
