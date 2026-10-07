# Build update — 2026-10-06

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

Validation: final build succeeded; both CTest suites passed (16 editor cases and 6 output cases, including suite setup/cleanup). Native desktop automation could not initialize its bundled runtime; visual review used the generated Qt screenshots. See `VERIFICATION.md` for coverage and limits.

Use `Run-Cuebase.cmd` for the rebuilt application. `Cuebase-build-update-2026-10-06.zip` contains buildable source, test fixtures, test results, screenshots and the local RTMP test receiver with its license. It is not a portable runtime installer.

The original `Cuebase-buildstudio-updated.zip`, user library and user media are preserved.
