# Timed split screens — implementation contract

Prepared 2026-10-15 for checklist task 37. This is a design specification, not an
implemented feature. It can be prepared while builds are stopped for disk space.

## User workflow

The viewport toolbar opens Layouts. Choose a layout, assign sources by thumbnail,
then choose Now or Schedule. Schedule places an editable layout event on the
broadcast timeline. Duration is shown in frames and timecode using the project's
frame rate. Preview edits remain private until explicitly applied.

The requested example is directly representable: two speakers from 00:00 to
00:10, one speaker from 00:10 to 00:15, then three speakers. Editing durations
moves boundaries visibly; it never stretches the underlying source footage.
Provide a keyboard-accessible toolbar as well as hover reveal.

## Layouts and five transition presets

Initial editable layouts: Solo, Equal pair, Three columns, Host plus two guests,
and Four-up grid. Store normalized rectangles and crop/focal points so output
resolution changes do not require rebuilding the layout.

Five original motion presets, proposed defaults rather than sampled broadcaster
timings:

| Preset | Behavior | Proposed duration |
|---|---|---|
| Glide | Existing slots move/resize together; new slots fade in | 12 frames at 25 fps |
| Soft dissolve | Blend old/new compositions without moving source clocks | 10 frames at 25 fps |
| Slide reveal | New cells enter from the nearest frame edge under a clip mask | 12 frames at 25 fps |
| Focus expand | Selected speaker grows to the main region; remaining slots recede | 14 frames at 25 fps |
| Staggered arrange | Slots reach their destinations in a short, ordered sequence | 16 frames at 25 fps |

Durations are editable and converted using the project frame-rate ratio. Reduced
motion substitutes a short dissolve or cut. Content never receives the decorative
UI catch-up animation. Interpolation is explicit; source playback is continuous.

## Document model and evaluation

- Layout preset: stable ID/version, name, slots and per-slot crop/border settings.
- Slot: stable identity, source reference, normalized rectangle, crop, fit mode,
  z-order, visibility and audio assignment. Matching uses identity, not label/order.
- Timeline event: stable ID, broadcast start frame, target layout snapshot,
  transition duration/type/easing and an explicit audio policy.
- Source registry: share decoded frames for repeated uses of the same source;
  detect recursive scene references and reject cycles with an actionable message.
- Evaluation: a pure function of broadcast time, event snapshots and source frames.
  Seeking samples the same result as forward playback. No accumulated animation
  timer state. Back-to-back transitions start from the evaluated outgoing geometry.
- Event conflicts: do not silently overlap two layout events; show the conflicting
  interval and offer move/replace/cancel. Define event end exclusively to avoid a
  duplicated boundary frame. Trim transition duration to its available interval.
- Missing source: retain its slot and show an authored fallback, with a diagnostic
  in the editor. Never substitute another camera silently.

## Audio policy

Default to **Keep current audio mix**. Visual rearrangement must not duplicate
audio from a source shown in two slots. Offer explicit Follow visible sources and
Custom routing only when the mixer can actually evaluate them. Preview audio is
monitor-only unless explicitly routed. Apply equal-power crossfades where an
authored audio route changes; do not reset source clocks or create a second player
merely because a source moves.

## Integration and acceptance

Use a shared compositor for Studio monitor, public program frame and export.
Keep the private preview cursor separate from event evaluation on air. Emergency
standby remains above this compositor and returns to the current broadcast time.
Persist events/presets with project data; editing an event is one undo transaction.

Required checks before task completion:

1. Two-to-one-to-three sequence at exact boundary frames, including rational fps.
2. Matching forward playback, direct seek, rewind and exported frame hashes.
3. Duplicate source in two cells produces one audio contribution.
4. Source crop/focal point, aspect ratio and z-order stay correct during transitions.
5. Missing/deleted sources, empty layouts, cyclic references and interrupted media.
6. Preview scrubbing and layout drafts leave public output unchanged.
7. Save/reopen, undo/redo, duplicate event, cancel and conflict resolution.
8. Hover and keyboard toolbar, high DPI, reduced motion and readable small cells.
9. Four simultaneous sources under a measured resource budget; no pretend camera
   support where a capture backend is unavailable.

## References

vMix documents reusable layered layouts with source positioning, crops, borders,
ordering and import/export. Cuebase's scheduled-event and audio rules above are
our engineering decisions based on the user's requested workflow.
[vMix Layers](https://www.vmix.com/help29/InputSettingsMultiView.html)

OBS documents explicit preview-to-program transition. Cuebase's private editing
must retain that separation while providing the requested live picture-in-picture.
[OBS Studio Mode](https://obsproject.com/kb/OBS-Studio-Overview#studio-mode)
