# Dragon Ball: Advanced Adventure — Android four-action HUD (Draft)

Based on the previously published Android touch/visual preset branch, **not**
on the performance-diagnostic branch. This is host UI only. The main ROM/BIOS
execution path, ARM64 shards, timing, gameplay speed, saves and input ABI remain
unchanged. **No Android APK was built or tested in this task.**

## Four independent controls

For this title's **Story Mode**, action circles map to the original GBA keys:

| Icon | Guest KEYINPUT bit | Key | Actual use |
| --- | ---: | --- | --- |
| Jumping silhouette / green | 0 | A | Jump (also double-jump where supported) |
| Glove/fist / red | 1 | B | Melee attack |
| Single Power Pole / gold/blue | 9 | L | Pole special attack (game decides Ki cost) |
| Ki orb / blue | 8 | R | Hold to charge and release Ki / Kamehameha |

SELECT, START and the existing D-pad are untouched. Multiple simultaneous
fingers are independently owned, OR-ed into the active-key mask and turned into
the original active-low KEYINPUT. Holding R over multiple frames is preserved
for genuine Ki charging. There is only **one** staff button; shoulder L/R pills
are omitted in this landscape layout. Pointer areas are the same circles as
the drawn action icons; they scale with the existing 80/100/120% setting and
follow safe insets/rail geometry. Portrait retains the generic controls.

The action glyphs are vector primitives (circles/lines/rounded rectangles),
not generated-image textures, bitmap assets or GPU shaders. Pressed states
change the backing and scale; the selected icon remains recognizable.

## Special-ready glow

A game-owned, read-only `SpecialMeterProbe` is called during presentation
with the **uncorrected RGB888** guest framebuffer. It returns -1 when it cannot
identify the gauge or size; the engine hides the ready glow in that case.

The title's current probe samples native coordinates x25..42, y146..148 of
the colored Ki bar, and checks its dark bottom border around y151. It counts
yellow **and** orange segments and returns the fraction filled.

The positions were calibrated against an existing 2520x1080 owner screenshot
whose actual native game image was x540..1980, y60..1020; a first-stage
bar appeared ~83% full; a separate captured frame showed it empty.
Synthetic tests of fully-filled segments return 100%. This is a **visual
heuristic**, **not a verified guest RAM address**. The filled-bar threshold,
UI modes and palette behavior must be validated during gameplay on the Moto
G100. Color profiles do not affect the probe because it samples raw pixels
before host color mapping. It returns unknown on non-240x160 sizes.

On the host the blue rings activate after **three frames >=96%**, turn off
after **three frames <94%**, and reset immediately when the gauge cannot be
recognized. The glow consists of two pulsing outline strokes, not a blur or
a costly shader. Both L and R specials glow only when the same shared gauge
appears completely charged. Their actual availability/damage remains wholly
controlled by the original game — the UI does not simulate or grant energy.

## Validation

- Compiled an isolated C++20 ROM-free meter probe with `-Wall -Wextra -Werror`
  and passed five synthetic cases: empty, partial, full, bad dimensions, frame
  lacking border.
- Measured visible fill fractions against three older screenshots of gameplay
  (about 0.83, 0.83 and 0.00); this supports, but does not prove, calibration
  against raw native output in all modes.
- Reviewed the native layout/input mapping, Android opts and synchronized
  engine pins. No Android/NDK build, render comparison or touch-latency
  benchmark has been performed.

Before merge: make **one manual Codemagic ARM64 build** of
`feat/dragonball-mobile-action-hud`. Verify landscape D-pad with moving
Goku; individual A/B/L/R actions, hold R to charge/release; multi-touch
movement+A+R, menu and Start/Select; glow with visibly depleted/full Ki bars,
then confirm it stops after a special consumes energy. If brightness is
inaccurate, adjust only the game-owned visual probe after calibration. Do not
change guest memory, invent RAM offsets, or claim a performance improvement.

Branch and both PRs remain Draft with no merge or automated builds.
