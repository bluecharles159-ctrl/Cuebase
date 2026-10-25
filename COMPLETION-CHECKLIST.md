# Cuebase completion counter

Updated 2026-10-25. **22 of 84 tasks completed — 62 remaining.**

The Claude transcript adds four distinct acceptance groups (81–84). Row 06 is
reopened: earlier checks covered transforms/material color, but inspection shows
that shape conversion still hides Background/Effects and loses full surface
rendering. The counter does not treat that narrower check as full completion.

These are fixed acceptance groups, not equal units of effort or a release-readiness
percentage. Partial implementation stays unchecked. Completed rows have recorded
regression evidence; the separate native UI, integration and release gates still
apply. REQUIREMENTS-STATUS.md and CREATIVE-STUDIO-PLAN.md retain the detailed
requirements. Both supplied briefs and the conversation are included; direct user
instructions take precedence. The unrelated portrait edit is excluded.

New requests will be added explicitly with a dated change to the total. Do not
silently remove, split or merge rows to make progress appear larger. After finishing
a row, update this file and report “X of 84 tasks completed — Y remaining.”

## Interaction and animation

- [ ] 01. Origin dots, positioning controls and overlay visibility; native review.
- [ ] 02. Apply-modifier action/shortcut, undo and stack behavior; native review.
- [x] 03. Mirror reference-object picker and cancel; modifier-card interaction. Actual card button, viewport selection, Escape, undo/redo and saved reference verified in `mirror-card-results.txt`.
- [x] 04. Enter/Return commits numeric values and releases input focus.
- [x] 05. Select All feedback without unintended transforms or hidden selection.
- [ ] 06. Parented Edit Mode world coordinates and full shape appearance preservation, including Background/Effects controls and rendering after conversion.
- [x] 07. Independent per-object inspector expansion, including text Typography defaults. Verified selection/default/persistence workflow in `portable-editor-results.txt`.
- [ ] 08. Independent frame-range cues, empty state, extraction and timeline highlighting.
- [ ] 09. Rounded cue creation/edit modal and polished cue-board cards/padding.
- [x] 10. Loop counts and independent per-play holds, timings and Studio controls. Verified finite/infinite edits and preserved per-play holds in `cue-loop-editor-results.txt` and `cue-loop-studio-results.txt`.
- [x] 11. Build manual hold/continue for individual cue plays.
- [ ] 12. Nested cue boards, duplication, delays, scheduling, trims and on-air edit protection.
- [ ] 13. Selected-only graph curves, active-curve emphasis and Translation labels; native audit.
- [ ] 14. Graph handles, interpolation, key transforms/copy/paste and navigation persistence.
- [x] 15. Orange modified key replaces the saved key; unchanged yellow key removes it.
- [ ] 16. Pointer-anchored canvas zoom, adaptive rulers/grid and native navigation review.
- [ ] 17. Parenting, isolated parent transforms, hierarchy search/reveal and rounded corners.

## Appearance and studio interface

- [ ] 18. Compact Inspector styling, indentation and collapsible groups across all panels.
- [ ] 19. Studio Blue default, retained presets, thin scrollbars and settings categories; full audit.
- [ ] 20. Distinct licensed icons and consistent rounded menus, hover states and modals.
- [ ] 21. Integrated title bar, Settings before Minimize and window/channel transitions.
- [ ] 22. Programs grid/list, thumbnails, metadata, playlists, search and saved-scene switching.
- [x] 23. Markdown text editing/rendering and animation-preserving reusable content fields. Inspector edits, undo/redo, saved formatting, reusable fields and PNG/render parity verified in `markdown-workflow-results.txt`.
- [x] 24. Independent glyph and background shadows for text objects.
- [x] 25. Text-background auto-fit with independently selectable X/Y axes.
- [x] 26. Solid/gradient/media backgrounds and backdrop-blur parity in Build/Studio/export. Inspector-to-Final-to-PNG comparisons pass in `export-parity-editor-results.txt` (53 cases); Studio reference comparisons, translucent backdrop and ordering checks pass in `background-studio-format-results.txt` (6 cases). Proof captures inspected. Comparisons use each renderer's native alpha format; display grading and wider lighting acceptance remain task 50.
- [x] 27. Overlay Z-order controls and editor/rendering agreement. Source and per-placement controls, saved overrides, inheritance, duplicate independence, snapshot order and save failure verified in `placement-order-results.txt` (5 passing cases).
- [x] 28. Circular imported logo containers, channel overview and editable/imported/built identity. Actual rounded profile dialog, draft previews, Cancel, Save/reopen and built-logo selection verified in `channel-profile-results.txt` (7 passing cases) and final polish in `channel-profile-polish-results.txt` (3 passing cases); dialog capture inspected.
- [ ] 29. Object naming shorthands for logos, continuity screens and reusable categories.

## Broadcast, preview and audio

- [ ] 30. Primary sequence supports actual programs/ads/elements, linked segments and exact timing.
- [ ] 31. Main-track delete/expand, cut icon, trim/split/ripple, snapping, transitions and validation.
- [ ] 32. Fixed-center Studio playhead, moving timeline, frame stepping and navigation; native audit.
- [x] 33. Source-aligned thumbnail strips/waveforms and persistent Timeline settings. Verified by `resumed-preview-handoff-results.txt` (9 passing focused cases, including both trim modes); native/performance release gates remain separate.
- [ ] 34. Current-program scrubber, private preview and continuously visible live picture-in-picture.
- [ ] 35. Return-to-live overlay/animation and independent replay controls; live-camera replay gap.
- [ ] 36. Black-screen override, looping alternate content, logo presets and editable/error messages.
- [ ] 37. Timed multi-source split screens and five smooth transition presets.
- [ ] 38. Control-panel tool navigation and wheel-adjusted volume controls with useful meters.
- [ ] 39. Audio workspace: multiple sources, trim/split, pan, fades/crossfades, duplicate and automation.
- [ ] 40. Sample-clock mixing, buses, synchronization, metering and high-quality audio export.
- [ ] 41. Reorderable/bypassable DSP chains: EQ, dynamics, filters, spatial/time/modulation effects.
- [ ] 42. Pitch/time processing, stereo imaging, sidechain, presets and processor automation.
- [ ] 43. RTMP/SRT sustained output, health/errors, recovery and public-destination validation.
- [ ] 44. Audience metrics with real provider data and dashboard verification.

## Planning board

- [ ] 45. Private mixed-source planner preview, filtering/list reorder, drag/drop and moving playhead.
- [ ] 46. Resizable/full-screen board, rich text/image cards, formatting, locks and resize/empty states.
- [ ] 47. Shared connection ports, timeline links, tracking, line styles and native gestures.
- [ ] 48. Planner undo/reopen, atomic saves, recovery, isolation and representative-media checks.
- [x] 49. Reviewed plan-to-rundown append, exact durations, save-failure rollback and undo. Verified in `rundown-portable-results.txt`, including instance controls and picture/audio lifecycle. Native and sustained-output gates remain separate.

## Creative expansion

- [ ] 50. Solid/Material/Rendered mode semantics, lighting/render parity and adaptive quality. Work/Material/Final selector, plain structural appearance, local persistence and unchanged final output verified in `build-modes-results.txt` (48 passing editor cases); captures inspected. Auto/Full/Half/Quarter quality, bounded buffers and backdrop reference comparison pass in the subsequent 50-case suite. Shared display grading for Final/clean preview/PNG, Work invariance and alpha preservation pass in `color-parity-editor-results.txt` (54 cases); Studio output passes 6 cases. Complete lighting/performance acceptance remains.
- [ ] 51. Configurable composition guides, margins, colors, responsive layout, dragging and snapping. Normalized persistent guides, ruler dragging, undo/redo, Clear, safe insets/grid/color dialog and JSON round-trip pass in `composition-guides-results.txt` (50 cases); captures inspected. Responsive object-layout constraints and full native acceptance remain open.
- [x] 52. Texture library, collections/favorites, reusable presets, missing-file repair and portability. Material panel, managed image imports, stable IDs, search/filtering, favorites, scene drag/drop, presets, undo/redo, save/reopen, missing repair, portable sharing and failed-save rollback pass 9 focused cases in `texture-library-verified-results.txt`; pixel blend checks and animated PNG equality pass. Full editor suite: 55 passing cases in `texture-editor-regression-results.txt`. Captures inspected. This covers 2D text/shape/image surface materials; layered/PBR materials remain task 53.
- [ ] 53. Ordered material layers, mapping/blends, shared styles and evaluated surface properties.
- [ ] 54. Editable shape/alpha/luma/gradient/text/brush masks, combinations, feathering and animation.
- [ ] 55. Shared text/logo appearance effects, reflections, bevel/extrusion, glass and light sweeps.
- [ ] 56. Nondestructive surface painting, brush controls, ordered layers, masks and undo.
- [ ] 57. Drawable editable effect splines, handles, moving-object attachment and animated paths.
- [ ] 58. Particle/mesh emitters, sparks, fire, smoke, fog, dust, debris, liquid and atmospheric presets.
- [x] 59. Pixel/block/liquid/scattered-piece reveals with smooth/constant/step/flicker timing. Actual Inspector controls, undo/redo, saved settings, repeatable backward scrubbing and exact PNG parity for five surface styles × five timing modes pass in `reveal-workflow-results.txt` (5 cases). Rendered proof sheet inspected. These are 2D surface effects; 3D emitters/fluid simulation remain in task 58.
- [ ] 60. Arbitrary transformed rigid bodies, collision filtering, custom shapes and simulation caches.
- [x] 61. Stable reusable components, :: registration, search/quick insert and linked source revisions. Actual Build/browser workflow and saved-library reopening verified in `component-verified-results.txt`; individual renderable objects, with explicit updates in the open scene.
- [x] 62. Local component overrides, selective propagation and default-linked animations/cues. Verified local/disabled groups, source ownership, duplicate cue IDs, stale review, rollback, undo and saved-instance reload. Scene-time cue and base-placement limits are recorded in REUSABLE-OBJECTS.md; independent object clocks and bulk cross-document updates are not implied.
- [ ] 63. Full docking/tabbing/floating/maximize, persisted layouts, presets and safe reset.
- [ ] 64. Master graphic templates, full-screen/lower-third/ID variants and shared brand tokens.
- [ ] 65. Portable model/element/animation/baked-cue import/export, assets and round-trip checks.

## Accounts, research and delivery

- [ ] 66. Studio-default onboarding, editing-only mode and production/channel details.
- [ ] 67. Google/GitHub/email accounts and real cross-device login — Supabase deferred by user.
- [ ] 68. One default channel, isolated channel data, multiple windows and optional shared settings.
- [ ] 69. Secure device pairing, revocation, permissions and device-management interface.
- [ ] 70. Multi-device document/media sync, concurrent roles, offline conflicts and change details.
- [ ] 71. Shared team notes, assignments, reminders and production scheduling.
- [ ] 72. At least one hour of multi-channel broadcast references and documented transition study.
- [ ] 73. HGTV renovation-promo Blender/Cuebase recreation, editable files and comparison proof.
- [x] 74. UI/product research and sourced professional-feedback report distinguishing recommendations.
- [ ] 75. Original Cuebase branding and promotional advertisements after functional acceptance.
- [ ] 76. Download/documentation website and verified Windows/macOS/Linux installers.
- [ ] 77. Free installation, three-month trial, monetization and authorized GitHub release.
- [ ] 78. Native/high-DPI/accessibility audit, integration/recovery/performance and clean-install gates.
- [x] 79. Persistent completion ledger and dependency-based architecture plan; retain unfinished scope.
- [x] 80. Local regression infrastructure for Build, Studio, rendering and localhost streaming evidence.

## Completion evidence

## Additional Claude transcript requirements — 2026-10-25

- [ ] 81. Non-destructive curve noise, selected-key/channel entry points, per-curve controls, persistence, undo and shared-renderer output.
- [ ] 82. Front/middle/back sub-layers, G/Z steps, delayed NEW-layer boundaries, cancellation, layer colors and overlap-only broadcast stacking.
- [ ] 83. Dual-camera layout, side-view toggle and final-camera aspect presets.
- [ ] 84. Boolean effects for text with preserved appearance and editable controls.

Other transcript items extend existing rows: interaction repairs (05, 16–17),
timeline history/copy/paste (14), snapping (16), work surround (50), video and
Markdown (23, 26), native tests (78). See CLAUDE-REQUESTS.md for the detailed
recovery scope. No inaccessible Claude output archive is counted as merged.

## Earlier completion evidence

74: Expanded STUDIO-FEEDBACK-RESEARCH.md on 2026-10-15 with primary product and
creator sources, UI recommendations, current source-level gaps, priorities and
difficulty. The research/report task is complete; implementation, the one-hour
video study and HGTV reconstruction remain separate unchecked tasks. Prepared
SPLIT-SCREEN-IMPLEMENTATION.md for task 37 without claiming it is implemented.

04–06 and 11: `VERIFICATION.md`, 38-case Build baseline and
`REQUIREMENTS-STATUS.md` immediate repair evidence. 15, 24–25: Studio Blue Build
regressions covering modified-key replacement, glyph shadows and axis-specific
text fitting. 79–80 concern tracking/test infrastructure only; they do not certify
the unfinished features or a release. The preview now passes; the trim test update
and interrupted application build still need verification after freeing disk space.
The intermittent RTMP issue remains open for sustained testing. Planner rows remain unchecked
despite passing automated checks because their listed acceptance work remains.
