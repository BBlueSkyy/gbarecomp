# Dragon Ball HUD navigation follow-up

Continues PR11 from bc32727a; companion game policy is required. No guest CPU,
HALT, continuation, snapshot, DMA, IRQ/SWI or generated-code changes.

The title HUD removes SELECT's visual geometry and hitbox. Empty pad rectangles
cannot claim a padded hit region at the origin. Other titles and physical
SELECT retain their existing mapping. One grey vector PLAY button replaces the
old START label; a separate grey hooked-arrow BACK is a title-owned UI control,
not Android Back. Pressed outlines/fills match the grey D-pad. No hide tab.

PLAY/BACK are `TouchEvent::control` edges, sent through TouchHub and VBlank to
the title policy, including Down+Up arriving between guest frames. They do not
enter the gesture recognizer, assert host pad KEYINPUT, or generate Tap/
LongPress settings shortcuts. Finger ownership remains fixed while sliding.
The game emits START bit3 and context-gated B bit1 as bounded two-frame presses
and two-frame releases. A fixed four-request FIFO preserves consecutive taps;
overflow drops excess requests. The title cancels queued BACK when its observed
screen changes. All other pad fingers retain their existing multi-finger path.

`clear_touches` publishes a title-only Cancel on runtime focus/lifecycle loss,
including after the short-tap finger has gone away. This invalidates already
drained policy requests before guest execution resumes. Platform Back's runtime
settings behaviour is unchanged.

The action skin is used in portrait too. If the lower band cannot contain its
stack, it may overlay the image rather than merge jump/punch/Ki hit regions.
Navigation adds only bounded vector spans/lines/arcs; no fonts, new per-frame
heap storage, framebuffer copies or shader effects. Existing rendering primitive
implementation costs remain unchanged.

Essential local validation uses SDL2 software/dummy drivers: coordinates across
landscape/portrait, typed raw edges for sub-VBlank HUD taps, no phantom SELECT
at origin, generic SELECT retained, and high-density compact portrait jump
asserting A alone (1080x1200,16px/mm). The companion game integration runs actual
VBlank/KEYINPUT and an authored ARM LDRH consumer, including rapid taps,
suspend/resume cancellation and multitouch. This is not an Android APK/device
performance test; no FPS improvement or regression-free phone result is claimed.
