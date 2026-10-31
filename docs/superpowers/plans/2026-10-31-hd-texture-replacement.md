# HD Texture Replacement Implementation Plan

**Goal:** Load authored HD tile art without changing guest execution or canonical frames.
**Architecture:** Content + palette fingerprints for BG/OBJ tiles, cached read-only replacement samples in the downstream high-resolution compositor. Host loader owns PNG/WebP decoding and bounded eager residency; unknown/invalid/missing assets retain native art. Runtime owns the pack and UI; Dragon Ball opts in.
**Constraints:** main bases 33e26c9 / 7502b6a; preserve touch, runtime, IRQ/HALT, snapshots, optimizations; Draft PRs only. PNG/WebP; conservative 3x presentation, 64 MiB decoded budget. No invented ROM fingerprints or widescreen world geometry.

## Tasks
- [ ] Pack + loader: test missing/invalid packs, content and palette mutation, BG/OBJ separation, dimension/path/budget validation and PNG/WebP decoding. Implement gba_texture_replacement.{h,cpp}, runtime/hd_texture_loader.{h,cpp}. No decoding or IO in the raster loop.
- [ ] Raster integration: tests through real PPU, verify native buffer unchanged, HD subpixels, priority, flips, blend/fade and disabled fallback. Extend high-resolution Pixel with optional tile sample, supply regular BG and non-affine OBJ metadata in wide compositor. Unsupported affine/bitmap rendering retains original art.
- [ ] Runtime + game: opt-in path and toggle/reload, preserve raw frame touch probes, export bounded native tile catalog for authoring. Package real demo art, reproducible real-PPU demonstration and mapping tool. Explicitly document unmapped real-game assets.
- [ ] Verify essential touch/save/HALT and HD checks, publish two Draft PRs, request existing ARM64 APK workflow and verify artifact when available. Record any build/network blockers honestly.

## Review focus
Untrusted paths and image dimensions; stale VRAM/palette matches after restores; image pointer lifetime during reload; transparency/window/priority preservation; overhead with no matches and disabled.
