# Verification — 2026-10-10

## Build

`cmake --build build-final --parallel 1` succeeded with MSYS2 UCRT64 GCC 16.2.0, Qt 6.11.2, Assimp 6.0.5 and FFmpeg 9.0.2 development libraries. The application is `build-final/Cuebase.exe`; `Run-Cuebase.cmd` supplies the installed runtime on PATH. The default preset now limits compiler/autogen jobs and uses compact debug information for this 4 GB machine. `Build-Cuebase.cmd` launches only after a successful build; Code::Blocks is not required.

## Tests

The final editor suite passed all 24 cases (11.63 seconds in CTest). The full Studio/output suite passed all 17 cases in 47.09 seconds. The final CTest run completed in 58.84 seconds. Both localhost RTMP and SRT recordings were decoded and replayed successfully. The text and JUnit reports record the final assertions and timings. The separate loopback-recheck.txt report is from the earlier baseline verification.

Earlier RTMP runs exposed an initialization stall: audio delivery was coupled to successful video writes, so video backpressure could starve FFmpeg's separate audio input. The feeds now advance independently with bounded queues. Startup also supplies bounded probe frames, tolerates first-use Qt event-loop pauses before streaming, and reports input initialization failures distinctly. Local receiver startup reads/writes allow 30 seconds; established output still detects stalls. The loopback fixture waits for its test tone to begin before publishing.

| Suite | Result | Coverage |
|---|---|---|
| CuebaseBuild | 24 passed, 0 failed | Cue-board repeat/hold boundaries, indefinite loops, independent text/designs, disabled children, Build cue extraction and variant persistence; animated screen-effect presets, exact Bezier key values and fade endpoints;  Base values/Auto Key; independent channels; default reset and diamond toggle; duplicated tracks; Bezier evaluation/persistence and mouse handle editing; parenting/overflow; horizontal-only inputs; color picker and animated context button; older JSON migration; camera navigation; placement cursor mouse clicks, undo/redo and white creation; mesh operations; gradients/rounding; Mirror/Bevel geometry; actual FBX/GLB fixtures; video decoding/seeking; audio timing; save/reopen and graph key dragging without playhead movement; persistent graph pan/zoom and multi-channel interpolation; single-transaction input undo/cancel; backdrop filtering, percentage corners, audio timing and named cue persistence; Build library autosave/switching; reusable fields, media validation and unchanged animation tracks; thumbnail and playlist persistence. |
| CuebaseStream | 17 passed, 0 failed | Cue-board private drafts, resolved live content, manual Continue/Hide and later scheduled trigger precedence; independent replay clock, opt-in pause/resume and end-of-rundown handling; actual recording replay; encrypted crew Viewer/Editor permissions, revocation, stale-edit rejection and atomic persistence;  Clean program rendering, off-air black, live Custom graphics, guide exclusion and pause behavior; endpoint validation; RTMP and SRT loopback publishing/receiving; H.264/AAC codec and resolution checks; decoded non-silent audio; exact overlay clip trimming and source offsets; entrance/hold/exit scheduling, Studio cue controls and paused animation rendering; private Studio drafts and Apply; live placement validation; metrics parsing and per-source history isolation; actual Studio layout construction. |

Qt totals include setup/cleanup cases. Text and JUnit results are in `build-final` (under `verification` in the source ZIP). Local received media is in `build-final/artifacts/loopback-rtmp.flv` and `loopback-srt.ts`.

RTMP used MediaMTX 1.21.1 on a temporary localhost port; SRT used FFmpeg's local listener. No station credentials or public destination were used. The exact Monitor receiver configuration was also started independently: its localhost metrics endpoint returned HTTP 200 and MediaMTX metrics.

## Visual review

Reviewed the new cue-board private preview, repetition fields, compact controls and scrollable Studio details, plus the scene-effects controls, rounded checkboxes and production/crew workspace, alongside generated screenshots of the compact split Build workspace, graph with edited Bezier handle, custom color picker, animation cue editor, full Studio workspace, reusable sponsor fields and rounded thumbnail dialog. Automated Qt mouse interaction checks ran with the offscreen platform. A separate native desktop check could not initialize the computer-use runtime (initialization timed out twice). Therefore native window/menu interactions are not claimed as manually verified.

## Practical limits

- The included receiver supports local tests. The user must supply a publicly reachable RTMP/RTMPS or SRT receiving endpoint for public distribution. Public station connectivity, RTMPS certificates, regional distribution and long-duration load testing were not exercised.
- Receiver connections are not unique viewers. History is collected while Cuebase is running, kept for 63 days, and separated by metrics URL and stream path. Cable/satellite ratings and CDN analytics require their own data providers.
- The source archive uses the installed Qt/Assimp/FFmpeg runtime; it is not a standalone installer. The built executable is available locally through the launcher.
- FBX/glTF imports cover editable geometry and base colors, not skeletal animation or arbitrary external shader/texture networks. Image/video/audio sources remain linked files.
- The renderer is a CPU composition/mesh renderer. Output resolution, composition complexity and machine speed affect sustained broadcast performance; the output engine reports overload/stall errors.
- Replay uses recorded timeline media, not a live-input DVR buffer. Crew sessions expose production plans and reusable text over a host-controlled LAN/VPN HTTPS session; they do not provide remote playout control or shared media storage.

## Baseline integrity

Original ZIP SHA-256, rechecked after implementation:

`8A14BFDB20C75955491EE7F673B0EE7A8F559FBF1FA59F53AF544BB2D40E6E6D`

The package excludes user library data, user media, credentials, temporary certificates, and old build directories.
