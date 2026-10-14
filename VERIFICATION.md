# Verification — 2026-10-14

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

Planning tests cover the independent data model only. There is no interactive
planning board yet. Native physics interaction, complex concave collisions and
the wider feature/parity audit remain incomplete; this is not a release sign-off.

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
