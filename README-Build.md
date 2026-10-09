# Cuebase Build

This update uses `Cuebase-buildstudio-updated.zip` as its source baseline. Build creates graphics for TV production: save a composition to **Screen Elements**, then use that Custom element in the existing Studio, preview, and broadcast workflow.

## Build and test

Requirements: C++20, CMake 3.25+, Ninja, Qt 6.8+ Widgets/Multimedia/Network/Test, Assimp, pkg-config, FFmpeg development libraries (avformat, avcodec, avutil, swscale), and FFmpeg with libx264, AAC, RTMP and SRT support. The supplied presets use the installed MSYS2 UCRT64 toolchain. RTMP loopback tests also use MediaMTX at `tools/mediamtx/mediamtx.exe`.

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
cmake --preset default
cmake --build --preset default --parallel 2
ctest --test-dir build --output-on-failure
```

Set `-DBUILD_TESTING=OFF` when configuring an application-only build. Test results are written as text and JUnit XML in the build directory. Tests use a separate temporary application identity for library persistence; they do not load the production library.

On this machine, open `Run-Cuebase.cmd` to launch the verified `build-final` executable with the installed Qt runtime on PATH. After rebuilding from the source archive, the same launcher also supports the preset's `build` directory. The source archive is a complete buildable project; it is not a portable runtime installer.

## Studio library and broadcast monitoring

Screen Elements now has playlist cards, private animation previews, thumbnail cropping, editable text/media fields and validated playhead-following placement. Build has a saved-element dropdown, autosaving New scene action and four animated starter presets. Use `{{studio:Sponsor}}Sponsored by NVIDIA` or the Studio Field checkbox to expose reusable content without changing its animation.

Studio Monitor can start the included localhost MediaMTX receiver and record real receiver connections by server/path over time. Public distribution needs a reachable server or CDN. See [PRODUCTION-GUIDE.md](PRODUCTION-GUIDE.md) for local RTMP/SRT URLs, audience-count limits and the full workflow.

## Composition editing

- New elements open with a white rectangle, a locked orthographic camera and a key light. The clean composition preview and editing viewport sit side by side, with Layers and Animation below. Drag splitter boundaries to resize panels.
- Shift + right-click sets the placement cursor. Alternatively, enable the crosshair tool and click in the editor. New shapes, text, meshes, images and imported models appear at that position. Cursor placement supports undo/redo and persists with the composition. View / Align includes cursor-to-center and cursor-to-active-object commands. The cursor is excluded from broadcast output.
- Middle/right drag pans the editor; the wheel zooms around the pointer. Fit, 100%, Home, and frame selected change only editor navigation.
- Drag a layer to move it. Bounding handles resize around the opposite handle; Shift or Maintain aspect ratio preserves proportions. The handle above the selection rotates it.
- Shift-click and drag a selection box to select multiple layers. Ctrl+D duplicates; Delete removes unlocked layers.
- Rulers, title/action safe areas, center guides, composition grid, snapping, and alignment/distribution are under **View / Align**. Drag from a ruler to create a guide; drag a guide out of the frame to remove it. Alt bypasses snapping during movement.
- Canvas X/Y are output-pixel coordinates. Depth and the advanced 3D transforms use the existing scene coordinate system (X horizontal, Y depth, Z up). Stored animation channels retain these scene axes for compatibility; graph values for position channels are scene units. Layout width/height and typography use design pixels, with 120 design pixels per scene unit at the reference plane.
- Camera properties become editable only after unlocking the camera in its inspector. Navigation never changes the camera.
- The Layers panel supports selection, visibility, locking, renaming, and drag ordering. Equal-depth surfaces preserve their layer order; real 3D geometry still sorts by depth.

## Animation

For named entrance/hold/exit cues, Studio timers, background audio and the updated graph controls, see [PRODUCTION-GUIDE.md](PRODUCTION-GUIDE.md).

Auto Key starts **off**. Ordinary edits change base values without creating animation keys. For an already animated property, an unkeyed edit is shown until the playhead moves; use Auto Key or Key Transform to update existing keys; an unfilled diamond creates a key for its property.

- Gray fields: no animation; green: keyed elsewhere; yellow: keyed at the current frame; orange: the current value differs from its key. The property diamond and an orange ring on timeline/graph keys also mark unrecorded changes.
- A property diamond inserts a key when unfilled and removes the key when filled at the current playhead. Auto Key records only changed channels. Key Transform is an explicit grouped command.
- Every key can be deleted, including time zero. Empty tracks are removed.
- Timeline Summary shows animated objects; the eye menu switches between all and selected objects. Expand layers and collapsed property groups to reach individual channels. Summary and group keys support movement, scaling, interpolation changes and deletion. Timeline and Graph Editor share the same `PropertyTrack` data.
- Shift-click or box-select keys; drag to move without moving the playhead. Scale Keys scales selected times, values and Bezier handles around the selection center. Delete removes selected keys. View All, Frame Selected, wheel zoom, middle/right pan, frame snapping, normalization, and the interpolation menu are available.
- Constant, Linear, Bezier, Smooth, Ease In, Ease Out, Back, and Bounce curves use the same evaluator as rendering. Select a Bezier key to expose its handles; drag a handle to shape the curve, or Shift-drag to align the opposite handle. Handle coordinates persist in saved elements and undo history.
- Space plays/pauses, K inserts transform keys, Alt+K deletes the current key, arrows navigate keys, and Ctrl+arrows step frames. Undo/redo includes property changes, animation edits, canvas transforms, mesh operations, and layer edits.

## Inspector and rendering

The inspector has collapsible sections, property key/reset controls, mixed-value display, and type-specific controls. The Add menu offers implemented 2D rectangles/ellipses, text, images, existing mesh primitives, and lights.

2D rendering supports solid/linear/radial/conic fills, independent rounded corners, stroke styles and joins, drop shadows, blur, brightness, contrast, saturation, grayscale, hue rotation, invert, sepia, and the exposed Qt blend modes. Text supports family, size, weight, italic, tracking, word spacing, line height, alignment, case transforms, and decoration. Images support Fill, Contain, Cover, None, and Scale Down with positioning.

The mesh renderer, lighting, and vertex/edge/face operations remain available. Mirror supports axes, bisect/flip, offsets, a reference object, clipping and vertex merging. Bevel supports edges/vertices, width modes, segments, angle limiting, profile and overlap clamping. Plane is in the 2D Add menu; shapes can be converted to editable geometry and extruded. 2D objects ignore lighting by default; 3D objects receive lighting. This is a software renderer, not a physically based DCC renderer.

Background controls select Solid, Gradient or Media. Gradients support linear, radial and conic fills. Media supports images/SVGs and decoded video, fit/position, trim offset, speed, looping, volume and image adjustments. Audio layers have start time and volume. Media remains linked to its original path, so keep those files available when moving a project. FBX, OBJ, glTF/GLB, DAE, STL and PLY import editable mesh geometry and base material colors; skeletal animation and external 3D texture networks are not imported.

File → Import and external file drops use the existing Build composition or open Build when needed. Video drops open a metadata/placement dialog and insert a Studio program at Start, End or a position chosen on the timeline. Right-click a Screen Element and choose the animated Build action to reopen its saved composition. File → Export saves composition JSON (`.cuebuild`) or the current frame as PNG. Quit offers Save, Don't Save and Cancel.

## Files and compatibility

- `ElementDesign`, `Animation`, and `DesignTexture`: base values, independent numeric tracks, rendering, and versioned serialization.
- `BuildModel`: editing, Auto Key, selection, undo/redo, and mesh operations.
- `BuildCanvas`: fixed-camera composition navigation and transforms.
- `PropertyInspector`, `LayerPanel`, and `AnimationEditor`: contextual controls, layer management, timeline, and graph editing.
- `BuildWorkspace`: integration with Screen Elements; `MainWindow`: routes Build undo/redo commands.
- `tests/BuildTests.cpp`: model, migration, camera, mesh, rendering, graph interaction, and save/reopen regression checks.

JSON version 5 stores creation defaults, parent relationships and inverse transforms, Bezier handles, playback start, `base`, per-property `tracks`, and rendered style data. Unversioned flat designs and version 2/3 bundled transform keys, plus version 4 property tracks, migrate on load, preserving their values and interpolation. Unknown style entries remain in the JSON. Undo snapshots and saved Custom elements use the same serializer. The supplied baseline ZIP remains untouched.

## Updated controls

Numeric inputs use horizontal dragging and horizontal wheel/trackpad scrolling. Shift-drag makes fine adjustments; double-click types a value. Vertical scrolling scrolls the panel without editing values. Reset restores the creation default, independent of the user's editable base value. Newly created layers capture their defaults; older files use the layer type's standard defaults.

The inspector stays on the last active layer after deselection. Eye and padlock icons control visibility and locking. The canvas toolbar and the Inspector/Modifiers/World selectors are fixed icon rails with tooltips and active states. Start/End frame inputs define the editor playback range; drag the playhead head to scrub. Help → Keyboard Shortcuts includes the Build and Graph Editor commands.

For parenting, select children first, then Shift-select the intended parent last and press Ctrl+P. Existing world transforms are preserved; future parent motion affects children. Alt+P clears parenting while preserving the current transform. Circular relationships are rejected. Overflow X/Y on a 2D parent independently clips descendants in both the canvas and broadcast renderer. Hidden parents hide descendants. Duplicated layers have independent animation tracks.

## Direct live RTMP / SRT output

Cuebase contains a local encoder/output engine; it does not require a Cuebase cloud backend. It sends the clean program picture and mixed playout audio to the RTMP/RTMPS or SRT endpoint provided by your broadcaster or streaming service. That receiving endpoint is external to Cuebase.

1. Set output resolution and frame rate in Display Settings.
2. Load the show and use Start playout.
3. Under Control → Network Output, choose Configure / start stream.
4. Enter the destination and optional RTMP stream key, confirm the FFmpeg executable and bitrates, then start.
5. The status reports Connecting, Streaming, reconnect attempts, or the actual encoder/connection error. Stop network stream stops transmission; Stop Broadcast also stops network output.

Video is H.264, audio is stereo 48 kHz AAC. RTMP uses FLV; SRT uses MPEG-TS. Output excludes editor controls, safe-area guides, preview scrubbing and pause labels. Color management, output region, live graphics and scheduled elements remain in the program feed. Main media and additional timeline sounds are mixed using their actual volume/mute controls. Decoder audio is captured through Qt's FFmpeg multimedia backend.

Connections retry up to three times with a two-second delay. Queue limits and progress timeouts detect stalled or overloaded encoding rather than reporting a false live state. Endpoint credentials remain in memory for the current session; encoder path and bitrates persist locally.

`tests/StreamTests.cpp` sends real video and a generated audio tone through local RTMP and SRT receivers. It checks H.264/AAC codecs, output dimensions and non-silent decoded audio. No station account or public endpoint is contacted by these tests.

Implementation references: [Blender parenting](https://docs.blender.org/manual/en/5.0/scene_layout/object/editing/parent.html), [Blender interpolation and handles](https://docs.blender.org/manual/id/3.6/animation/keyframes/introduction.html), [Qt painter clipping](https://doc.qt.io/qt-6/qpainter.html), [Qt decoded audio capture](https://doc.qt.io/qt-6/qaudiobufferoutput.html), and [FFmpeg RTMP/SRT protocols](https://www.ffmpeg.org/ffmpeg-protocols.html).
