# HD texture replacement v1

This is a real art replacement path, independent of scaling filters. PNG/WebP
art samples enter the downstream scanline compositor before the existing
window/priority/alpha/brightness resolution. The guest 240×160 framebuffer,
VRAM, OAM, input probes, CPU, collision, DMA/HALT and snapshots are not edited.

## Pack

Games opt in with `RunOptions::hd_texture_pack = "hd/title"`. Other games keep
existing behavior. `pack.json` has version 1, exact game `rom_sha1`, and `tiles`:

```json
{"version":1,"rom_sha1":"GAME_SHA1","tiles":[
  {"key":"0123456789abcdef","file":"tiles/leaf.png"}
]}
```

Keys: FNV-1a 64-bit, offset 14695981039346656037, multiplier 1099511628211.
Feed bytes `[1, obj?1:0]`, then 64 unflipped tile pixels in row-major order.
Each pixel is a little-endian uint16: 0 for palette index zero; otherwise
`(palette_color & 0x7fff) | 0x8000`. VRAM address, palette slot and bit depth
are absent from the identity; equivalent decoded tiles match across moves.
BG and OBJ have distinct namespaces. Native palette changes produce different
keys and fallback; recolored variants must be authored explicitly.

Images are square RGBA PNG/WebP, 16..128 pixels, dimensions divisible by 8.
The replacement keeps the original tile's 8×8 logical coverage. Files resolve
inside the canonical pack directory; traversal/symlinks outside are rejected.
At most 16,384 entries, 4 MiB manifest, 2 MiB compressed tile, 64 MiB resident
RGBA. Loading is eager and bounded: decode/file IO never runs in the raster.
Cache entries verify raw tile and palette bytes, including after state restores.
Malformed/missing images are skipped; a missing/empty/wrong-ROM pack retains
original art. Duplicate keys are rejected. PNG uses vendored stb; Android WebP
uses bounded BitmapFactory via JNI; Linux WebP uses optional system libwebp.
Other hosts currently fall back for WebP; PNG remains available.

## Controls and authoring

Runtime Graphics offers **HD textures** and **Capture tiles for HD art**.
Enable capture before visiting a scene. Up to 1024 distinct native tiles are
held in memory and exported as 8×8 PNG plus `catalog.tsv` in
`hd/title/catalog` on orderly runtime exit. Capture is opt-in, not persistent,
and adds raster work; disable it for FPS measurements. Unknown tiles retain
original art. Enable HD textures while capturing; both controls are session
settings. Set `GBARECOMP_HD_PACK`, `GBARECOMP_HD_DISABLED=1`, or
`GBARECOMP_HD_CAPTURE=1` for desktop automation. Restart after installing art.
The game companion includes a pack builder using captured keys.

## Scope and limits

- Regular 4bpp/8bpp tiled backgrounds and non-affine sprites support replacement;
  these cover tile-based scenery, characters, effects and HUD alike.
- Affine/bitmap backgrounds and affine OBJ retain original art in v1.
- Hardware-transparent source pixels stay transparent. HD alpha blends with
  the original tile pixel; it cannot reveal an arbitrary third layer. Paint
  within the original silhouette. Art cannot extend beyond native tile bounds.
- Original priority/windows govern visibility. No automatic grouping of a
  multi-tile character, animation guessing, or frame overlay is performed.
- Effective HD presentation is at least 3x; decoded pack residency is capped at
  64 MiB. Moto G100 frame time has not been measured here.
- Aspect-correct presentation is preserved. This does not invent off-screen
  level geometry or enable retail-game widescreen without a verified adapter.

The companion's **HD ART DEMO** runs the real PPU on an authored fixture, without
ROM/BIOS. Its keys deliberately do not claim to map retail Dragon Ball assets.
The initial game pack is empty until those original tiles are captured/mapped.

## Verification

ROM-free pack mutation/disable/namespace tests, PNG/WebP load checks, and
high-resolution compositor subpixels passed. Real PPU checks cover unchanged
native bytes, BG/OBJ, priority, flip, alpha and brightness. Essential touch,
presentation layout, high-resolution affine, continuations, HALT and SWI
snapshot regression targets passed. Both headless runtime and SDL+recomp-ui
runtime compiled locally. Android artifact validation is recorded on the PR.
