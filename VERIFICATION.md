# Verification — 2026-10-08

## Build

`cmake --build build-final --parallel 2` succeeded with MSYS2 UCRT64 GCC 16.2.0, Qt 6.11.2, Assimp 6.0.5 and FFmpeg 9.0.2 development libraries. The application is `build-final/Cuebase.exe`; `Run-Cuebase.cmd` supplies the installed runtime on PATH.

## Tests

Both CTest suites passed on the final application build. The last full run passed CuebaseStream in 34.57 seconds; after correcting the editor test to use persisted graph coordinates, CuebaseBuild passed in 46.81 seconds. Zero failures in these final suite runs.

| Suite | Result | Coverage |
|---|---|---|
| CuebaseBuild | 19 passed, 0 failed | Base values/Auto Key; independent channels; default reset and diamond toggle; duplicated tracks; Bezier evaluation/persistence and mouse handle editing; parenting/overflow; horizontal-only inputs; color picker and animated context button; older JSON migration; camera navigation; placement cursor mouse clicks, undo/redo and white creation; mesh operations; gradients/rounding; Mirror/Bevel geometry; actual FBX/GLB fixtures; video decoding/seeking; audio timing; save/reopen and graph key dragging without playhead movement; persistent graph pan/zoom and multi-channel interpolation; single-transaction input undo/cancel; backdrop filtering, percentage corners, audio timing and named cue persistence. |
| CuebaseStream | 10 passed, 0 failed | Clean program rendering, off-air black, live Custom graphics, guide exclusion and pause behavior; endpoint validation; RTMP and SRT loopback publishing/receiving; H.264/AAC codec and resolution checks; decoded non-silent audio; exact overlay clip trimming and source offsets; entrance/hold/exit scheduling, Studio cue controls and paused animation rendering. |

Qt totals include setup/cleanup cases. Text and JUnit results are in `build-final` (under `verification` in the source ZIP). Local received media is in `build-final/artifacts/loopback-rtmp.flv` and `loopback-srt.ts`.

RTMP used MediaMTX 1.21.1 on a temporary localhost port; SRT used FFmpeg's local listener. No station credentials or public destination were used.

## Visual review

Reviewed generated screenshots of the compact split Build workspace, graph with edited Bezier handle, custom color picker, animation cue editor and Screen Elements cue controls. Automated Qt mouse interaction checks ran with the offscreen platform. A separate native desktop check could not initialize the computer-use runtime (`failed to write kernel assets: The system cannot find the path specified`). Therefore native window/menu interactions are not claimed as manually verified.

## Practical limits

- The user must supply a working RTMP/RTMPS or SRT receiving endpoint for a real broadcast. Public station connectivity, RTMPS certificates, regional distribution and long-duration load testing were not exercised.
- The source archive uses the installed Qt/Assimp/FFmpeg runtime; it is not a standalone installer. The built executable is available locally through the launcher.
- FBX/glTF imports cover editable geometry and base colors, not skeletal animation or arbitrary external shader/texture networks. Image/video/audio sources remain linked files.
- The renderer is a CPU composition/mesh renderer. Output resolution, composition complexity and machine speed affect sustained broadcast performance; the output engine reports overload/stall errors.

## Baseline integrity

Original ZIP SHA-256, rechecked after implementation:

`8A14BFDB20C75955491EE7F673B0EE7A8F559FBF1FA59F53AF544BB2D40E6E6D`

The package excludes user library data, user media, credentials, temporary certificates, and old build directories.
