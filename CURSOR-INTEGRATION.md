# Cursor integration — 27 October 2026

Cursor checkout: `C:/Users/JusticeNick/Documents/GitHub/Cuebase`.
Its base is `8a45503` (the initial project), while the current application is on
`codex/cloud-build`. Cursor's 11 changed/new source files are preserved verbatim
under `reference-implementations/cursor-2026-10-27/`; they are reference source,
not duplicate compilation units. The original checkout is not overwritten until
the combined changes have been checked and backed up.

## Integration map

| Cursor change | Current application integration |
| --- | --- |
| WorkCanvas drag-to-create rectangles, edge/centre guides, off-camera selection | Adapted to BuildCanvas and its existing scene transforms, renderer and undo. Ellipse drawing, Shift constraints and Escape cancellation added. |
| PlanBoard solid notes and shadows | Ported to PlanningCanvas while retaining rich text, links, images, undo and the independent production plan. |
| ToastHost icon notices | Adapted to shared theme/icons, pixel-based ellipsis, full accessible text and window resizing; wired to Build audio imports and saves. |
| MediaCache reusable previews | Current decoder retained; waveform source ranges and eight filmstrip sample positions now independent of zoom. Persistent caching still pending. Cursor's raw compressed-byte waveform is preserved as reference, not used as audio amplitude. |
| Data cut history | Current millisecond offsets and break/ad metadata retained; consecutive cuts support undo/redo. Unrelated program edits invalidate this cut-only history. A unified Studio history remains open. |
| AudioAsset/TimelineAudio/shared selection data | Preserved; shared library and scene-audio migration still pending. Existing audio rendering must remain compatible. |
| TimelineWidget header | Preserved; its changed constructor lacked matching implementation in Cursor's checkout. C-to-cut and header scissors are integrated with the existing timeline; broader audio-selection wiring remains open. |
| Visual noise/bounce | Preserved as reference; current curve noise and Bullet physics retained. Modifier category UI remains open. |

## Validation

Cloud tests are required before calling these integrations verified. No local
compilation is being run. This is a partial integration record, not a claim that
the full backlog or all Cursor features are complete.

Update, 28 October: the shared Studio/Build audio library, in-scene audio list,
drag-to-place audio, instance speed/volume/offset/fades, modifier/physics filter,
and persistent decoded thumbnail/waveform cache are implemented. Scene audio is
kept out of visual layer lists and canvas selection; its stored representation
remains compatible with existing projects. Echo processing remains unfinished.

Cloud run 40633835200 compiled successfully: both editor runs passed 17 checks
and Build passed 55. Streaming passed 40 checks with one screenshot assertion
failure caused by the newly tinted background. The correction and additional
audio-selection/offset guards await the next batched verification run.

Additional requests still open include professional logo, right-side settings,
default readable timeline width policy, full color-preview coverage outside the
Inspector, and notice coverage for all media actions. The table above records
the initial integration state; this update supersedes its pending audio, cache,
and modifier-filter entries. Overall verified completion remains 23 of 84.

References: [Figma snapping](https://help.figma.com/hc/en-us/articles/360039956914-Adjust-alignment-rotation-position-and-dimensions)
and [shape tools](https://help.figma.com/hc/en-us/articles/360040450133-Shape-tools).
