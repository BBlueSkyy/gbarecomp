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

## Second visual correction based on the actual installed APK

The owner tested the first version on Moto G100; its D-pad was still dark and
bulky, the arrows were thin chevrons, and the charge light never activated.
This branch now uses lighter glass-tinted D-pad tiles, layered borders and
filled triangular arrows; blue lighting has four concentric translucent neon
rings only for charged R/Ki and L/staff. The punching and jumping buttons keep
faint warm red and green borders without falsely indicating charged specials.

**Root cause of absent charge glow found in the game's probe:** the old sample
span x25..42 included white endcap pixels x41..42. The actual gauge's yellow
charge interior is x24..39, so a full bar could only give 15/18 (83.3%) to a
detector with a >=96% readiness requirement. The updated game-owned probe
samples x24..39 and verifies fixed cap, silver edge, and lower black border,
then applies a three-frame readiness/depletion debounce. On two prior Moto G100
gameplay screenshots, this correctly classifies full (1.0) and empty (0.0).
The new synthetic tests also cover partial, full, empty and invalid border.
**No on-device build of the corrected version has yet been run.**

## Visual redesign to match the approved mobile mockup

The initial vector proof-of-concept built only rudimentary circles and icons;
the owner's screenshot showed its mismatch with the approved picture. This
branch now replaces the placeholder *entire mobile-skin drawing path* with
`src/runtime/mobile_touch_skin.h`:

- **D-pad**: four independent dark glass-like translucent directional tiles
  with silver outlines, arrow glyphs, a central hub, and thin pale-blue
  concentric rings (same original directional hit detection).
- **R/Ki**: blue enamel, white-hot orb and several cyan energy streaks.
- **B/punch**: red enamel, peach clenched fist with individual knuckles,
  thumb and wrist.
- **L/Power Pole**: brown enamel and one diagonal orange/gold staff with
  highlights and dark outlines (only one such button; no L shoulder pill).
- **A/jump**: green enamel, white leaping fighter silhouette and arrow,
  to the left of the staff, never overlapping its touch circle.
- Thin metallic outer rims and layered translucent highlights on every
  action; subtle permanent border, with **intense pulsing cyan/blue outlines
  only at confirmed full Ki gauge**. Pressed buttons visibly compress.
- START and SELECT also receive smaller translucent dark glass pills.

All art is drawn procedurally through the existing SDL/ImGui HostOverlay.
It does **not** reuse pixels, cropped art or a generated PNG from the concept
image; it is a deliberately lightweight approximation of the approved styling.
The host can draw icons without font support, internet, external files or a
new shader backend. The style is only enabled in Dragon Ball's opt-in
`mobile_action_hud`; other games keep the generic pad. A ROM-free fake
HostOverlay smoke test for idle/ready/pressed drawing lives in
`tests/runtime/mobile_action_skin_test.cpp`.

**Verification still required**: The new vector skin was reviewed statically
but has not been compiled or visually verified on Android. Pixel-for-pixel
identity with the concept image is not claimed. Inspect screenshot differences
on the Moto G100 and adjust dimensions, alpha and icon strokes if needed.

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

## Punch and jump: final user-supplied icon references

The latest owner feedback explicitly selects two **close-up reference images**
for the red punch and green jump controls. The previous procedural
`fist_icon()` and `jump_icon()` only approximated them with straight
rectangles and connected line segments. They have been replaced by compact
64×64 1-bit **reference-traced silhouettes** embedded in
`src/runtime/mobile_reference_icons.h` (64-bit rows for punch and jump).
The raster masks derive from the two images supplied by the owner; there
is no ROM/BIOS content, network image request or runtime asset loader.
A horizontal-run renderer batches contiguous foreground mask pixels into
one HostOverlay rectangle per run instead of drawing each pixel. The fist
uses a warm cream/orange gradient; jump uses pale mint/white and a solid
rising arrow, while the red and green glass/enamel circular backplates and
metallic rims from `mobile_touch_skin.h` remain.

This makes the actual in-game icon contour follow the chosen pictures
significantly more closely than the old simple vector placeholders. It
does not imply pixel-identical rendering: the references were resampled
to 1-bit 64px silhouettes with two-tone colors.

Input is unchanged: B still punches, A still jumps, and multitouch and
hitboxes are unchanged. R/Ki and L/Power Pole remain charged-state blue,
and the read-only RGB888 meter from the game PR is untouched.

ROM-free host overlay smoke tests also check that >150 icon-mask
horizontal runs are drawn and that the bright-blue rings appear for
both specials only in ready state. Full APK compilation and actual
Moto G100 appearance/perf tests remain pending. On the Android build,
frame-to-frame icon rendering adds around 200 short horizontal spans
per frame (no external asset upload, no new shader); FPS overhead must
be verified rather than assumed negligible.

## Four-key translucent grey D-pad reference (October 2026)

The owner clarified that the directional control is **not a circular
joystick**. The final reference has four separate rounded-square buttons
in a cardinal cross (up, down, left, right); each contains an *outlined*
five-sided directional arrow. The gap in the middle stays empty. No outer
circle, central knob, blue ring or filled arrows.

The updated `mobile_touch_skin.h` renders four individually bounded
buttons: neutral grey with 32/255 translucent fill and 118/255 border at
idle, a slightly brighter pressed state, grey outlined glyphs. Rounded
corners are constructed using short geometric segments even on the SDL
fallback without rounded strokes. Existing input geometry is deliberately
unchanged: bit 6 up, 7 down, 5 left and 4 right, diagonal gestures and
multitouch still use the same hit tests.

The four action circles have also received lower alpha on their dark glass
layers; the selected fist/jump contours and charged-only blue special
halo remain. Host-supplied reference image was only used for the shape,
not to replace the game framebuffer. The ROM-free visual smoke test now
checks four idle keys, 20 outlined arrow sides, 48 rounded key-outline
segments, a single highlighted key on Up, and no analog center circle.

This integration branch is based directly on the HALT-fixed engine main
(commit ff112660). It preserves the PR #10 HALT resume and DMA wake timing,
PR #9 owned IRQ/SWI continuations, and the runtime memory/SWI optimizations.
Only the host-side presentation changes are added, with 51 additive lines in
runtime.cpp. The companion game Draft stays based on its HALT-fixed main.
Actual touch-render FPS and visual appearance still require on-device tests.

## Unobtrusive pad visibility edge-tab (revision from actual APK screenshot)

The owner rejected the floating dark gamepad-glyph toggle previously seen
at the lower-right, circled in the Android screenshot. Replaced **for this
title only** with a narrow grey translucent **fixed top-right edge tab**:
3.2 mm by 9.5 mm, located within Android safe insets, with a small chevron
pointing outward to hide the virtual pad, inward to restore it.

A slightly expanded hit region (2 mm on all sides) makes it tappable without
enlarging the visible artwork. The tab is still drawn with the virtual pad
hidden; a prior `pad_visible=0` setting can no longer strand the player
because the tab remains accessible. Showing/hiding is saved in the existing
`config.ini [Touch]` format. No draggable icon, no old saved toggle position,
and no gameplay button mapping or special-meter changes.

Other game profiles keep their previous draggable gamepad toggle. The
engine's merged HALT/DMA performance path remains unchanged. Visual/functional
testing on the Moto G100 and a new Android APK build are still pending.
