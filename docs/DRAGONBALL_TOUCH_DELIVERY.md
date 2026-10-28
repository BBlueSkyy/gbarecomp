# Existing PR11: touch delivery qualification

This stage continues dfe4cddffd6b11188c16f1401f5295f48fa8e73d, preserving its HUD,
icons, independent grey D-pad, glass action buttons, glow, multitouch and removed
visibility toggle. No old runtime file or HALT implementation is copied.

The real SDL software test exposed double coordinate mapping on desktop native
views: SDL_RenderSetLogicalSize rewrites/clamps finger coordinates, then the
HostWindow route reapplies the drawable viewport. In the portrait regression a
tap at native y95 reached the policy at y122.22, while several menu choices were
outside the logical image. The larger game pipeline had11 failures before this
change and zero afterward.

For an opted-in touch policy, desktop now consistently uses explicit destination
viewports, including configure, resize/surface changes, coordinate publication,
render copy and top anchoring. This matches the existing Android path, which
already keeps SDL logical size disabled. Non-opting native desktop games retain
the historical logical renderer. The Android rendering/HALT/IRQ/DMA paths are
unchanged; this fix is not evidence that SDL remapping caused the Moto G100 issue.

`host_touch_coordinates_tests` exercises the actual SDL event watch, HostWindow
and TouchHub:18 Down/Up edges across2520x1080,1920x1080 and1080x2400 map once to
the requested native points. It is only built for desktop SDL-enabled hosts.
Existing touch_input_tests/presentation_layout_tests passed. The companion game
PR8 additionally tests SDL->VBlank->GbaIo->authored ARM KEYINPUT reads for DATA,
GAME and PAUSE, short/held touch and single confirmation:732 VBlank callbacks,
30 Downs,30 Ups, zero failures. Full findings, captured raster provenance and
remaining limitations are in its docs/MENU_TOUCH_DELIVERY.md.

The source also explicitly decays the noncapturing dispatch-observer lambda
with unary plus. The base's lambda/nullptr conditional fails GCC13 compilation;
this preserves the same function pointer/behavior and lets the published test
sources build without an unpublished workaround. Flags and guest timing are
unchanged.

No APK/NDK build, Moto G100 verification, FPS claim, remote build, Actions
monitoring, merge or generated shard change. The game-side classifier and
selection-acknowledgement changes are not engine HLE or guest-memory edits.
Original game OPTION styling and map interactions remain pending; no placeholder
UI or unverified map coordinates are shipped. The game synchronizes both manual
Android engine pins to this PR's new exact commit.
