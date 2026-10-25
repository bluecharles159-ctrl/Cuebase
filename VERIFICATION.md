# Verification — updated 2026-10-25

## Independent GitHub Windows build and functional tests

[Run 40193557415](https://github.com/bluecharles159-ctrl/Cuebase/actions/runs/40193557415)
tested commit `90323611ef98789943b029d88845a1a994a93aaf` on a GitHub-hosted
Windows runner. All six CTest suites passed: planning, textures, editor
interactions, components, Build and Studio/streaming. CTest took **30.34 seconds**.
The Build suite reports **55 passed**, Studio/streaming **40 passed**, and the
separate native Windows interaction run **9 passed**; none failed or skipped.
Localhost streaming includes RTMP and SRT, with MediaMTX pinned by checksum.

The staged runtime passed its editor and component tests with MSYS2 removed from
PATH. Recursive PE import checking resolved required DLLs, and ZIP integrity was
checked before upload. Artifact **Cuebase-Windows-1.zip**, 134,563,950 bytes:
SHA-256 `c751aab902727c08d233be31453bc547cb45fad89d314d167800f30c14d10f4a`.

Interaction checks cover deterministic noise and JSON persistence, independent
curve drafts, cancel/remove/undo, toolbar entry and frame export, duplicate
cancellation, mouse ownership, single-click selection, Alt-wheel history, and
off-camera workspace visibility with Final cropping. The off-camera regression
exposed the shared renderer's implicit camera-sized overflow clip; it now uses
the actual paint viewport when outer clipping is disabled, preserving parent
overflow constraints. The duplicate fixture counts original objects rather than
assuming the default scene has no light.

The initial local test compile failure and two failing local assertions are
superseded by this passing GitHub run. Native QTest interactions are automated;
they are not a manual visual audit or hardware compatibility certification.
Task 81 is complete. The corrected ledger is **23 of 84 completed, 61 remaining**.

## Reusable objects — first long brief, section 8

`component-repair-build.log` passes the serial application, component, editor,
texture and Studio test build. `component-verified-results.txt`: **12 passed,
0 failed** (15.1 s). Actual workspace registration, `/` browser opening, search,
insertion, metadata editing and library reopen are covered. Other cases exercise
selective source updates, default and disabled links, local overrides, independent
cue IDs, duplicate/delete/undo, source revision ownership, saved instance reload,
self-reference remapping, missing media, stale review rejection and failed-save
rollback. Animation-only propagation extends scene duration without changing cues.
Opening the browser on an unlinked object leaves the serialized scene untouched.

The first run exposed a failed-save mutation caused by a non-const JSON lookup;
it is fixed and covered. The stale-review fixture now changes a serialized
transform rather than an unused Shape text field. Tests use a unique isolated
profile and remove their own data. Component browser and update-review screenshots
in `artifacts/` were inspected; they are Qt offscreen captures, not native desktop
interaction evidence. REUSABLE-OBJECTS.md records the individual-object, explicit
current-scene propagation and scene-time cue scope, including remaining limitations.

Completed October 22 regression run, recorded October 24:
`component-editor-regression-results.txt`: **55 passed, 0 failed** (139.5 s);
`component-texture-regression-results.txt`: **9 passed, 0 failed** (2.3 s);
`component-studio-regression-results.txt`: **4 passed, 0 failed** (13.0 s).
Studio checks include textured backgrounds across display color views and
retained backdrop transparency. Tasks 61–62 are complete within the documented
scope: **23 of 80 completed — 57 remaining**.

## Texture library — first long brief, section 2

`texture-library-verification-build.log` passes the final serial app/editor/texture
build. `texture-library-verified-results.txt`: **9 passed, 0 failed** (3.2 s).
Checks exercise actual panel filtering, collection and favorite editing, applying
to selected unlocked objects, drag/drop onto the canvas, undo/redo, managed copies,
save/reopen, preserved content/transforms/animation, portable preset export/import,
missing-file repair and deliberate metadata-save failure with rollback and cleanup.
Rendering checks cover mapping, rotation, animated offsets, text glyph textures,
numeric Multiply/Screen/Overlay blend results, tint alpha, Work invariance, the
Solid-versus-retained-gradient preset edge case and exact Final/PNG equality after
a scene JSON round trip. Earlier 8-case run also passes in `texture-library-results.txt`.

`texture-editor-regression-results.txt`: **55 passed, 0 failed** (77.2 s). Captures
`artifacts/texture-library-panel.png` and `artifacts/texture-final-frame.png` were
inspected. Task 52 is complete within the surface-material scope documented in
TEXTURE-LIBRARY.md. Full native-app acceptance, mesh UV/PBR layers, painting and
cross-device media synchronization remain separate open requirements. Fixed count:
**21 of 80 completed — 59 remaining**.

## Light hierarchy and output parity

Final rendering now excludes lights hidden through a parent group, using the same
effective-visibility rule as scene objects. `lighting-parity-build.log` passes the
serial app/editor/Studio build. `lighting-parity-editor-results.txt`: **55 passed,
0 failed**, including Point/Sun/Spot/Area lights, Work/Material invariance under
lighting changes, parent-hidden versus directly-hidden light equality, saved-scene
and PNG equality, and animated light-power export. The inspected proof sheet is
`artifacts/build-lighting-modes.png`. `lighting-parity-studio-results.txt`: **6 passed,
0 failed**. Task 50 remains open for performance and native acceptance; this does
not increase the fixed completion counter.

## Display color parity

The existing Studio color transform is now shared through CuebaseBuildCore.
Build Final, the clean preview and PNG exports apply view/look/exposure/gamma
after rendering; Work/Material remain ungraded. Both Build previews subscribe to
display changes for repainting. `color-parity-build.log` passes the serial
app/editor/Studio rebuild. New editor checks cover all 12 view/look combinations,
Work invariance, clean-preview color, export equality and preserved alpha. Studio
checks exercise its actual output for all three views over each background type.
`color-parity-editor-results.txt`: **54 passed, 0 failed** (215.7 s). The color
proof capture was inspected. `color-parity-final-build.log` passes the final
incremental rebuild. `color-parity-studio-results.txt`: **6 passed, 0 failed**
(17.5 s), including each display view over Solid/Gradient/Media backgrounds,
translucent backdrop rendering and saved timeline-copy ordering. The counter
remains **20 of 80**; broader lighting/performance and native UI gates remain open.
See COLOR-MANAGEMENT.md for pipeline scope and transparent-compositing details.

## Build/output background parity and motion-blur export

The Studio target rebuilt successfully in `build-studio-parity-build.log`.
The initial sandboxed `studio-parity-results.txt` records **4 passed, 1 failed**.
The sandbox prevented saving the isolated application profile outside the
workspace. With that required access, `studio-parity-authorized-results.txt`
passes **5/5**, including timeline-copy order Save/reopen, inheritance, duplicate
independence, stale-edit handling and deliberately forced save failure.

PNG sequence and still export now pass project motion-blur settings into the
renderer. `build-export-parity-build.log` passes the app/editor build.
`export-parity-editor-results.txt`: **53 passed, 0 failed** (185.1 s), including
an animated frame that differs with motion blur enabled and exactly matches the
exported PNG, and Solid/Gradient/Media background Inspector-to-preview/export
parity with a translucent blurred overlay. Gradient and image proof captures
were inspected. `background-studio-format-build.log` passes the Studio rebuild;
`background-studio-format-results.txt`: **6 passed, 0 failed** (6.7 s), including
exact Solid/Gradient/Media reference comparisons and saved timeline-copy ordering.
The first comparison used different alpha buffer formats and failed; using the
same native format for reference and output passes. Studio's gradient capture
was inspected against Build's capture. Task 26 is complete: **20 of 80 completed
— 60 remaining**. Build/output color-management and broader lighting parity
remain open under task 50. These are offscreen checks, not a native UI audit.

## Surface reveal acceptance

`reveal-workflow-build.log` passes the test rebuild. Focused
`reveal-workflow-results.txt`: **5 passed, 0 failed** (56.8 s). The new Inspector
workflow exercises five styles (Wipe/Pixels/Blocks/Liquid/Assemble) and five timing
modes (Linear/Smooth/Constant/Steps/Flicker), undo/redo, serialized settings,
backward scrubbing and exact exported PNG equality for all 25 combinations.
Existing reveal endpoint/determinism and frame-export checks also pass.
Inspected `artifacts/reveal-styles-proof.png`, showing early/mid/completed frames.
See REVEAL-EFFECTS.md. Task 59 is complete for these 2D surface effects; no fluid,
volume or mesh-emitter simulation is claimed. **19 of 80 completed — 61 remaining**.

## Preview quality, backdrop rendering and composition guides

`composition-guides-build.log`: application and editor test builds passed.
`composition-guides-results.txt`: **50 passed, 0 failed** (74.7 s). Tests exercise
actual ruler dragging, guide movement after a resolution change, normalized JSON
round-trip, undo/redo, Clear guides, rounded settings Save/Cancel, and safe-margin,
grid and color settings. Inspected `artifacts/composition-guides-dialog.png` and
`artifacts/composition-guides.png`.

Build now draws its composition through a bounded image buffer so backdrop blur
can sample underlying pixels, including in the small clean-preview panel. Final
uses project motion-blur settings. Auto quality uses half resolution during
playback/modal transforms and full resolution when idle; Full/Half/Quarter are
also available. The automated canvas check compares the translucent red/blue
boundary to a direct renderer reference, verifies idle restoration, quarter-size
rendering, unchanged scene JSON and the 2,073,600-pixel cap after repeated zoom.
Inspected `artifacts/build-backdrop-preview.png`.

Initial `preview-quality-results.txt` had 48 passes and one overly strict sample
threshold away from the blur boundary; the renderer-reference comparison passed.
The corrected boundary sample and all remaining checks pass in the 50-case run.
These are offscreen workflow checks; sustained performance, complete lighting
parity and responsive object-layout constraints remain open. Counter remains
**18 of 80 completed — 62 remaining**.

## Build Work / Material / Final modes

`build-modes-build.log`: application and editor tests built successfully.
`build-modes-results.txt`: **48 passed, 0 failed** (66.3 s), full editor suite.
The new workflow test exercises the actual mode menu, keyboard cycling, selected
action synchronization, local preference restoration, neutral Work pixels,
unchanged source JSON and unchanged final output after switching modes. Work is
the initial workspace mode. Material uses neutral mesh lighting with authored
flat-surface textures; Wireframe stays separately available. Work texture copies
use separate cache identities so they cannot overwrite final texture entries.

Inspected `artifacts/build-work-mode.png` and `artifacts/build-final-mode.png`:
same composition/frame, gray structural surface versus authored blue gradient,
readable text and unchanged layout. These are automated offscreen captures, not a
native UI audit. See BUILD-VIEW-MODES.md. Item 50 remains partial: adaptive quality,
broader lighting parity and measured performance acceptance remain outstanding.
Counter remains **18 of 80 completed — 62 remaining**.

## Channel profile and circular identity

`channel-profile-build.log` and `channel-profile-polish-build.log` complete the
application and Studio test builds. `channel-profile-results.txt`: **7 passed,
0 failed** (51.9 s), covering profile workflow, placement ordering, cue-board
drafts and Studio layout. Final heading polish: `channel-profile-polish-results.txt`,
**3 passed, 0 failed** (20.7 s). The actual dialog verifies circular alpha corners,
live draft image changes, unchanged header before Save, Cancel, persisted name and
description, and selection of a built composition. `artifacts/channel-profile.png`
was visually inspected. This is automated offscreen coverage, not native desktop
acceptance. Task 28 complete: **18 of 80 — 62 remaining**.

## Build save recovery and named categories

`element-save-build.log` completes the application and both test executables.
`element-save-editor-results.txt`: **47 passed, 0 failed** (49.2 s), the full
editor suite. `elementNamingSaveFailure` forces a write failure using an isolated
test destination, verifies no library notification or changed saved name/category/
channel-logo assignment, retains the unsaved name tag, retries, and checks the
persisted main logo and subsequent break-screen classification. Ordinary Build
saves now use `saveChecked`, restore prior library state on failure, and only mark
the document saved after success. Canceling a new element's presentation dialog
also retains the requested name tag. `ELEMENT-NAMING.md` documents the supported
composition tags without implying they generate complete clock/replay systems.
This is supporting reliability work; task 29's broader object/category workflow
remains open. Counter unchanged: **18 of 80 — 62 remaining**.

## Editor regression rerun

`mirror-expansion-build.log` confirms the corrected test rebuild.
`editor-oct20-results.txt`: **46 passed, 0 failed** (89.9 s). The Mirror card
workflow now explicitly expands the card before checking visibility and using
the picker. This resolves the test setup failure recorded below. The placement
acceptance suite remains 5/5 passing. Counter: **17 of 80 — 63 remaining**.

## Per-placement overlap order

`placement-order-build.log` completes all 56 build steps for the app and both
test executables. `placement-order-results.txt`: **5 passed, 0 failed** (77.8 s).
Actual dialog controls preserve draft isolation, per-copy overrides, duplicate
independence, save/reopen, source/snapshot inheritance, conflict rejection and
rollback on save failure. Rendered frame colors confirm the effective ordering.
Task 27 complete: **17 of 80 completed — 63 remaining**.

`placement-editor-results.txt`: **45 passed, 1 failed** (302.0 s). The Mirror
card-picker test assumed its section was expanded, although sections default to
collapsed. The test now explicitly opens MIRROR before checking/clicking its
picker. The corrected test rebuild and rerun are still pending; this checkpoint
does not claim a clean editor regression suite or a release-ready application.

The records below describe earlier 2026-10-17 checkpoints.

## Mirror modifier card picker

`mirror-card-build.log` confirms the editor test build. `mirror-card-results.txt`
passed **4/4** (30.8 seconds). The test opens the Modifiers tab, uses the visible
card's picker button, cancels with Escape without changing the scene, selects a
different mesh through the viewport, and checks undo/redo and the saved reference.
The existing modifier apply/stack/Edit Mode checks also pass. Task 03 complete:
**16 of 80 completed — 64 remaining**. Native shortcut/appearance review remains
separate and does not change the open status of tasks 01/02/78.

## Markdown editor workflow

`markdown-workflow-build.log` confirms the application/editor build.
`markdown-workflow-results.txt`: **6 passed, 0 failed** (20.7 seconds).
The actual Inspector content and formatting controls preserve Markdown source,
Studio field registration, and animation keys. Undo/redo restores the document;
saved/reopened formatting and reusable content updates retain animation. A PNG
frame matches the shared renderer at the same timestamp. Existing Markdown
rendering, reusable-field validation, and inspector expansion checks also pass.
Task 23 complete: **15 of 80 completed — 65 remaining**. This does not close the
separate native appearance, cross-device or release gates.

## Cue-board shared themes

`cue-theme-build.log` confirms the build; `cue-theme-results.txt` passed **5/5**.
Reviewed offscreen captures `artifacts/studio-cue-board-studio-blue.png`,
`artifacts/studio-cue-board-darker.png`, and `artifacts/studio-cue-board-light.png`.
The board now inherits shared compact controls, colors and hover styling. Its
preview card and text use palette roles instead of conflicting fixed colors.
Loop/hold editing and overlay ordering still pass. This is a focused visual check,
not the outstanding native/high-DPI or application-wide appearance audit.

## Overlay save recovery and media conflict handling

`overlay-media-rebuild.log` confirms the build. `overlay-order-results.txt`
passed **4/4**: rendered overlap order, changing the actual Studio Z-index input,
independent element values, reopening saved data, and restoring both the model
and input after an induced save failure in an isolated test profile.
`media-conflict-results.txt` passed **3/3**: relinking, undo, clearing a filtered-out
selection, and refusing a stale draft after the underlying composition changes.
The earlier note that the stale-draft branch lacks coverage is now superseded.
Broader placement/snapshot ordering and native interaction remain to be audited;
tasks 27, 52 and 65 remain open. Counter remains **14 of 80 — 66 remaining**.

## Cue loop counts and per-play timing

`cue-loop-count-rebuild.log` confirms Cuebase and both test executables built.
`cue-loop-editor-results.txt`: **44 passed, 0 failed** (37.9 seconds).
`cue-loop-studio-results.txt`: **7 passed, 0 failed** (6.9 seconds).
The Build modal preserves existing infinite cues and independently edited holds;
positive counts become finite sequences. Studio verifies zero for infinite repeat,
independent hold durations, disabling/re-enabling Loop, and saved sequence timing.
Rundown and placement continue controls and cue picture/audio lifecycle also pass.
Task 10 is complete: **14 of 80 completed — 66 remaining**. Native visual review
and sustained output remain separate release gates.

The earlier `media-cache-results.txt` checkpoint passed 6/6. Image textures now
refresh after missing files are restored, replaced or removed at the same path.
Video decoder cache keys also include existence, size and modification time;
video restoration itself still needs a dedicated integration check.

The checkpoints below retain their completion counts as historical records.

## Composition media relinking

`media-relink-build.log` confirms the application and editor test build succeeded.
`media-relink-editor-results.txt`: **42 passed, 0 failed** (41.0 seconds). The
new relinking test selects a replacement through the Qt file picker, checks the
private draft leaves the model unchanged, applies both shared references without
losing animation keys, then undoes back to the exact original document. Invalid
replacement and cancel paths are covered. The dialog rejects stale drafts when
the model changed elsewhere; that conflict branch still needs a dedicated test.

The new window is available in Build's Add → Media → Composition media. Search,
missing-only filtering and path/status listing are implemented. Native window
verification is still unavailable. Tasks 52/65 remain partial; the fixed total is
unchanged at **13 of 80 completed — 67 remaining**.

## Portable compositions, inspector state and rundown replay

`portable-composition-rebuild.log` confirms Cuebase and both test executables built.
`portable-editor-results.txt`: **41 passed, 0 failed** (51.3 seconds), including
portable media deduplication, relocation, image/audio contents, cancellation after
a copy, preservation of existing exports, relative audio import, rejection without
replacing the open composition, and per-object inspector defaults/persistence.

`rundown-portable-results.txt`: **12 passed, 0 failed** (9.1 seconds), including
explicit seek/replay resetting rundown commands, separate placement controls,
private-preview isolation, picture/audio-player lifecycle, reviewed handoff, exact
durations, missing-media rejection, undo and save-failure rollback. The initial
audio test counted auxiliary monitor players; it now checks players referencing
the actual cue audio file. No hardware audio latency or sustained stream guarantee
is implied by the player lifecycle checks.

Task 07 and task 49 are complete: **13 of 80 completed — 67 remaining**. Portable
composition export is a verified part of tasks 52/65; their wider library, relink,
model/baked-animation and Studio cue-board packaging scope remains open.

Native computer-use initialization succeeded, but `list_apps` timed out twice and
again after a session reset. No native app input occurred. The native audit remains
open. C: had approximately 19.5 GB free before the build.

Earlier checkpoints below are historical; the remaining-instance-control notes
in them are superseded by this checkpoint.

## Rundown graphic cue controls

`rundown-cue-build.log` confirms successful application and Studio test builds.
`rundown-cue-results.txt` reports **9 passed, 0 failed** (30.3 seconds). The new
test operates the rundown Continue/Hide buttons and verifies copy/source
isolation, nonzero timeline start and source offset, stop/restart, reordering,
off-air command rejection and deleted-source rejection. Existing placement
controls, private cue drafts and all focused handoff cases also passed.

Animation commands now share instance resolution for library elements, placements
and the currently cued/airing standalone program graphic. The cue-board editor
shows separately labelled rundown controls for the matching airing graphic.
Native interaction, replay/seek policy and end-to-end audio agreement remain
separate acceptance work; this does not close the larger planner/cue groups.
Progress remains **11 of 80 completed — 69 remaining**. C: had about 6.1 GB free.

## Resumed build and regression checkpoint

After space was freed, `resumed-verification-build.log` completed successfully:
Cuebase, CuebaseRender, CuebaseBuildTests and CuebaseStreamTests were rebuilt.
`resumed-editor-results.txt` reports **38 passed, 0 failed**, and
`resumed-planning-results.txt` reports **6 passed, 0 failed**.
`resumed-studio-results.txt` reports **32 passed, 1 failed**. Both timeline-trim
variants and RTMP/SRT checks passed. The sole failure was a thumbnail arriving at
34.4 seconds after a 30-second test deadline. The preview test now allows 90 seconds
for cold decoding; this does not change the application's decoder behavior.
`preview-timeout-test-build.log` completed successfully. The subsequent
`resumed-preview-handoff-results.txt` reports **9 passed, 0 failed** in 31.4 seconds,
including source-aligned previews, both trim modes, handoff, nested design media,
save-failure rollback and the review dialog. This focused rerun resolves the
thumbnail deadline failure; it is not a claim of a newly repeated full Studio run.

Further handoff validation now checks graphics' object image/video paths, audio
paths, compiled cue variations and broken board references before mutating the
rundown. Its new regression passed and `handoff-assets-build.log` confirms the
application and test executable rebuilt successfully. Task 49
remains open, including program-instance cue controls. No native interaction or
sustained-output acceptance is claimed by these automated checks.

The October 15 notes below are historical; their stopped-build status is superseded
by this checkpoint. Approximately 0.8 GB remained free before this follow-up build.

Task 33 is now checked: **11 of 80 completed — 69 remaining**. The remaining
planner instance-control work and shared portable-asset/relink infrastructure are
still open. No dependencies or media downloads were needed in this pass.

## In-progress handoff and media-preview checks

The new planner handoff has focused coverage for reviewed append, exact durations,
standalone graphics/continuity screens, undo invalidation and save-failure rollback.
`build-final/handoff-focused-results.txt` reports 10 passing cases. The subsequent
full handoff Studio run reported 30 passed and one RTMP publishing failure;
`handoff-rtmp-recheck.txt` passed when run separately. That intermittent failure
remains an acceptance issue, not a clean full-suite result.

Timeline thumbnail/waveform decoding and settings have been implemented but are
still under verification. The first decoder test failed, then passed independently.
Stronger rendered-strip checks exposed delayed thumbnails and an overly strict
antialiased-edge color assertion; a full run timed out in that test. Decoder
tracing and captures confirmed the complete strip and source-aligned waveform.
The corrected assertion checks amplitude away from the silence baseline.

The subsequent `timeline-studio-final-results.txt` run reported **31 passed,
1 failed**, including passing preview/settings, planner handoff and RTMP/SRT
checks. The remaining failure was `timelineTrim`, whose mouse coordinates still
targeted the old lane height. Its updated test uses the clip's actual rectangle
and covers previews enabled and disabled. That change is **not yet verified**.
The earlier intermittent RTMP failure remains a sustained-output acceptance issue.

The application/test/renderer rebuild was stopped on October 15 because C: had
only 66,723,840 free bytes. No user files or build directories were deleted. Free
at least 3 GB before resuming `cmake --build build-final --target Cuebase
CuebaseStreamTests CuebaseBuildTests CuebaseRender --parallel 1`, then rerun Studio
and Build regressions. `planner-timeline-app-build.log` is an interrupted build,
not a success. The main application executable still predates this checkpoint.
Offscreen captures of `timeline-source-previews.png` and `timeline-settings.png`
were inspected; native interaction remains unverified.

User-facing completion counts are maintained in `COMPLETION-CHECKLIST.md`.

## Rigid-body and planning-model checkpoint

Rebuilt Cuebase, CuebaseRender and CuebaseStreamTests successfully with one
compiler job. **38 Build, 23 Studio and 6 planning-model cases passed**, including
local RTMP/SRT. Results: `build-final/physics-full-results.txt`,
`physics-studio-results.txt` and `planning-results.txt`.

Physics checks cover centered support, an overhang tipping and falling, flat
canvas-plane objects, deterministic backward scrubbing, scene-copy independence,
parented/world-space equivalence, moving passive supports, and gravity/ground
changes invalidating cached results. Five exported comparison frames were
generated; frames at 0, 2 and 4 seconds were visually inspected. The centered
block remains supported and the overhanging block lands on the ground.

This earlier checkpoint covered the independent planning model only; see the
interactive planner checkpoint below. Native physics interaction, complex concave collisions and
the wider feature/parity audit remain incomplete; this is not a release sign-off.

## Interactive planner checkpoint — 2026-10-15

The full Studio suite passes **27 cases**, including RTMP/SRT loopback and four
new planner checks. Results: `build-final/planning-studio-results.txt` and XML.
Focused checks also passed (`planning-ui-results.txt`). After the final add-menu,
window-geometry and bounded-image-cache refinements, Cuebase and its test target
rebuilt successfully; all 6 focused cases passed (`planning-ui-final-results.txt`). They exercise first-use
creation, mixed-source timing, private/live playback isolation, note edits during
playback, undo/redo, persistence, card drag/link gestures, crew-setting preservation,
and invalid-plan recovery while retaining the original data.

Inspected `build-final/artifacts/production-board.png` and `planning-canvas.png`.
These are automated offscreen captures, not native desktop interaction evidence.
A native check was attempted on October 15; the computer-use runtime failed with
"failed to write kernel assets: The system cannot find the path specified."
The board supports private preview, mixed-source sequencing, rich-text/image cards,
connections and checked saves. Native interaction, representative media coverage,
plan-to-broadcast promotion and broader release acceptance
remain pending. This is a development checkpoint, not a finished release.

## Studio Blue / editor checkpoint

Built `Cuebase`, `CuebaseBuildTests` and `CuebaseStreamTests` with one compiler job.
The complete CTest run passed: **36 Build cases and 23 Studio cases**, including
localhost RTMP/SRT. Total CTest time: 149.14 seconds. Reports are in
`build-final/studio-blue-ctest.log`, `test-results.txt/xml` and `stream-results.txt/xml`.

New checks cover orange-key replacement, text fitting by axis and glyph shadows,
deterministic surface reveals, channel isolation, pointer-anchored canvas zoom,
four theme presets and distinct icons, Settings/window-control ordering and
maximize/restore. The complete run also verifies the earlier modifier, picker,
per-play cue timing and Enter-focus repairs. A graph teardown use-after-free
found by GDB during this pass was fixed before the passing run.

Offscreen Qt screenshots were visually inspected for the Inspector, option lists
and Studio Blue workspace. These do not constitute native Windows window-manager,
high-DPI, clean-install or complete production-readiness verification. Native edge
resize, system dragging and snapping still require an interactive check.

The planning board, upgraded rigid-body solver, particle/mesh reveal sources and
the remaining expansion requirements are not complete. Supabase remains deferred
at the user's request. No final release archive has been produced.

## Previous checkpoint — 2026-10-12

## Build

`cmake --build build-final --parallel 1` succeeded with MSYS2 UCRT64 GCC 16.2.0, Qt 6.11.2, Assimp 6.0.5 and FFmpeg 9.0.2 development libraries. The application is `build-final/Cuebase.exe`; `build-final/CuebaseRender.exe` renders compositions to PNG sequences; `Run-Cuebase.cmd` supplies the installed runtime on PATH. The default preset now limits compiler/autogen jobs and uses compact debug information for this 4 GB machine. `Build-Cuebase.cmd` launches only after a successful build; Code::Blocks is not required.

## Tests

The editor suite passed all 29 cases (38.74 seconds in CTest). The Studio/output suite passed all 23 cases (75.04 seconds in CTest). The combined run completed in 114.03 seconds after adding fixed-center timeline navigation and correcting decimal project frame-rate conversion for Build export. Both localhost RTMP and SRT recordings were decoded and replayed successfully. The text and JUnit reports record the final assertions and timings. The local-receiver-check.txt report is from the earlier receiver verification.

Earlier RTMP runs exposed an initialization stall: audio delivery was coupled to successful video writes, so video backpressure could starve FFmpeg's separate audio input. The feeds now advance independently with bounded queues. Startup also supplies bounded probe frames, tolerates first-use Qt event-loop pauses before streaming, and reports input initialization failures distinctly. Local receiver startup reads/writes allow 30 seconds; established output still detects stalls. The loopback fixture waits for its test tone to begin before publishing.

| Suite | Result | Coverage |
|---|---|---|
| CuebaseBuild | 29 passed, 0 failed | Reveal endpoints and animation persistence; PNG frame content and timing manifest, cancellation and non-empty-folder protection; Per-object inspector expansion; Markdown rendering and persistence; Modal cue creation and temporary ranges; selected-cue playback independent of the scene range;  Cue-board repeat/hold boundaries, indefinite loops, independent text/designs, disabled children, Build cue extraction and variant persistence; animated screen-effect presets, exact Bezier key values and fade endpoints;  Base values/Auto Key; independent channels; default reset and diamond toggle; duplicated tracks; Bezier evaluation/persistence and mouse handle editing; parenting/overflow; horizontal-only inputs; color picker and animated context button; older JSON migration; camera navigation; placement cursor mouse clicks, undo/redo and white creation; mesh operations; gradients/rounding; Mirror/Bevel geometry; actual FBX/GLB fixtures; video decoding/seeking; audio timing; save/reopen and graph key dragging without playhead movement; persistent graph pan/zoom and multi-channel interpolation; single-transaction input undo/cancel; backdrop filtering, percentage corners, audio timing and named cue persistence; Build library autosave/switching; reusable fields, media validation and unchanged animation tracks; thumbnail and playlist persistence. |
| CuebaseStream | 23 passed, 0 failed | Overlay Z-index composition; Baked board isolation and five-minute live edit protection; independent placed-cue Continue; private preview preserving live output; translucent Studio backdrop composition; emergency picture/audio override;  Cue-board private drafts, resolved live content, manual Continue/Hide and later scheduled trigger precedence; independent replay clock, opt-in pause/resume and end-of-rundown handling; actual recording replay; encrypted crew Viewer/Editor permissions, revocation, stale-edit rejection and atomic persistence;  Clean program rendering, off-air black, live Custom graphics, guide exclusion and pause behavior; endpoint validation; RTMP and SRT loopback publishing/receiving; H.264/AAC codec and resolution checks; decoded non-silent audio; exact overlay clip trimming and source offsets; entrance/hold/exit scheduling, Studio cue controls and paused animation rendering; private Studio drafts and Apply; live placement validation; metrics parsing and per-source history isolation; actual Studio layout construction. |

Qt totals include setup/cleanup cases. Text and JUnit results are in `build-final` (under `verification` in the source ZIP). Local received media is in `build-final/artifacts/loopback-rtmp.flv` and `loopback-srt.ts`.

The Studio suite now also covers fixed-center scrolling in a real scroll area, middle-drag navigation without seeking, paused following and F to resume, viewport resizing, rational frame stepping, Fit and persisted mode settings. Export tests cover conversion of a decimal project base (30 / 1.001) to the reduced integer ratio 30000 / 1001 and rejection of a zero base. Inspected the current offscreen Studio screenshot with the playhead centered at time zero.

RTMP used MediaMTX 1.21.1 on a temporary localhost port; SRT used FFmpeg's local listener. No station credentials or public destination were used. The exact Monitor receiver configuration was also started independently: its localhost metrics endpoint returned HTTP 200 and MediaMTX metrics.

## Visual review

Reviewed the dark cue-board preview with aligned title and compact timing cards, live picture-in-picture with Return to live, translucent backdrop rendered over program video, repetition fields, compact controls and scrollable Studio details, plus the scene-effects controls, rounded checkboxes and production/crew workspace, alongside generated screenshots of the compact split Build workspace, graph with edited Bezier handle, custom color picker, animation cue editor, full Studio workspace, reusable sponsor fields and rounded thumbnail dialog. Automated Qt mouse interaction checks ran with the offscreen platform. A separate native desktop check could not initialize the computer-use runtime (initialization timed out twice). Therefore native window/menu interactions are not claimed as manually verified.

The channel header now uses a circular container for imported or saved-element logos. The code builds, but a native desktop visual check of that control remains outstanding.

The frame exporter uses the same scene renderer as Build and Studio. It exports picture sequences with alpha, not an audio mix, Studio rundown or scheduled cue-board bake. Additional assertions cover a radial reveal's visible center and hidden corners, a moved reveal center, and opening a composition with a relative image texture. The HGTV study is a separate two-second editable reconstruction and does not establish feature parity with a broadcast network. Its comparison report and reference media remain outside the application package.

## Practical limits

- The included receiver supports local tests. The user must supply a publicly reachable RTMP/RTMPS or SRT receiving endpoint for public distribution. Public station connectivity, RTMPS certificates, regional distribution and long-duration load testing were not exercised.
- Receiver connections are not unique viewers. History is collected while Cuebase is running, kept for 63 days, and separated by metrics URL and stream path. Cable/satellite ratings and CDN analytics require their own data providers.
- The source archive uses the installed Qt/Assimp/FFmpeg runtime; it is not a standalone installer. The built executable is available locally through the launcher.
- FBX/glTF imports cover editable geometry and base colors, not skeletal animation or arbitrary external shader/texture networks. Image/video/audio sources remain linked files.
- The renderer is a CPU composition/mesh renderer. Output resolution, composition complexity and machine speed affect sustained broadcast performance; the output engine reports overload/stall errors.
- Replay uses recorded timeline media, not a live-input DVR buffer. Crew sessions expose production plans and reusable text over a host-controlled LAN/VPN HTTPS session; they do not provide remote playout control or shared media storage.

- Production accounts, isolated multi-channel windows, native multi-machine scene editing and timed multi-source split-screen layouts remain unfinished. Existing host-controlled crew workflows do not imply a hosted account service.

## Baseline integrity

Original ZIP SHA-256, rechecked after implementation:

`8A14BFDB20C75955491EE7F673B0EE7A8F559FBF1FA59F53AF544BB2D40E6E6D`

The package excludes user library data, user media, credentials, temporary certificates, and old build directories.
