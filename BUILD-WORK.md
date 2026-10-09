# Build update — 2026-10-08

Implemented and rebuilt in `build-final/Cuebase.exe`:

- White defaults; default rectangle; orthographic composition camera; clean preview beside the editor; resizable Layers/Animation panels below.
- Placement cursor tool and Shift+right-click, cursor-to-center/active commands, undo/redo and serialized cursor position; creation/import uses that position.
- Original-value reset, current-frame diamond toggle, independent duplicated animation and duplicated hierarchy references, explicit Auto Key, Bezier interpolation and draggable handles.
- Collapsed animation groups, all-object Summary and selected-object tracks, zoom without seeking, key selection without seeking, playback range and playhead head.
- Gray/green/yellow/orange property states and edited-key rings; horizontal scrubbing; compact controls, icon rails, last-active inspection, eye/lock icons, grouped Help shortcuts.
- Last-selected active parenting with keep-transform and overflow X/Y clipping, including rounded/elliptical parent boundaries.
- Functional Mirror and Bevel controls; editable FBX/OBJ/glTF and other mesh imports; shape conversion and mesh operations.
- Solid/gradient/media backgrounds, decoded video textures and audio playback, image adjustments, custom color picker, linked/individual corners.
- File import/export, external-drop overlay, video placement dialog, animated Build context action, saved composition reopening, save-before-quit and save-before-replace dialogs.
- Clean program output and local FFmpeg H.264/AAC RTMP/RTMPS/SRT publishing with bounded queues, reconnect status and mixed audio.
- Named animation ranges with entrance replay, final-frame hold, exit/hide, manual triggers and program/ad-relative timers in Studio.
- Multiple background audio tracks with independent volume, speed, offsets, looping and fades; media creation through Shift+A.
- Shared graph/grid coordinates, persistent pan/zoom, Ctrl-drag axis zoom, multi-channel interpolation, handle types and key clipboard operations.
- Backdrop filters, rounded text backgrounds, percentage corners, parent-only transforms, layer search and drag parenting.
- Duration-accurate Studio overlay clips with trim handles and source offsets; animations pause with the program clock.
- Screen Elements search/categories, compact cue controls, transaction-based numeric dragging, cancellation and Enter-to-commit.

- Build library switching/autosave, New scene, thumbnail crop and metadata dialog, four animated presets.
- Studio playlist cards, private previews, reusable text/image/video fields, long-text fitting, validated placement and simplified Show/Exit controls.
- Resizable Studio monitor/editor/control row above the full-width navigable timeline; compact line icons and corrected countdown.
- Local MediaMTX receiver, live metrics, per-source history and date-range chart; no fabricated audience figures.

Validation: final build succeeded; 21 editor cases passed. The latest Studio/output run passed 12 cases and failed RTMP startup; SRT passed. Further local RTMP diagnosis is pending permission. Native desktop automation could not initialize its bundled runtime; visual review used the generated Qt screenshots. See `VERIFICATION.md` for coverage and limits.

Use `Run-Cuebase.cmd` for the rebuilt application. `Cuebase-studio-update-2026-10-08.zip` contains buildable source, test fixtures, test results, screenshots and the local RTMP test receiver with its license. It is not a portable runtime installer.

The original `Cuebase-buildstudio-updated.zip`, user library and user media are preserved.
