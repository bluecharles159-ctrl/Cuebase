# Animation frame export

In Build, choose Export and select **PNG animation folder (.frames)**. The current scene range and project frame rate are exported at the configured output resolution. Each file is a rendered frame with transparency; `sequence.json` records the size, rational frame rate and exclusive end frame. Choose a new or empty folder. Cancel stops after the current frame and keeps completed files, with no success manifest.

This is a picture export. Audio, a live program underneath a screen effect, and Studio cue-board scheduling are not included. The frame sequence can be encoded or composited with separately mixed audio. It preserves the authored scene animation; it does not claim to bake an entire Studio rundown or cue board.

The console renderer is built alongside Cuebase:

```
CuebaseRender.exe -platform offscreen scene.cuebuild output.frames --width 1920 --height 1080 --fps 30000 --fps-base 1001 --first-frame 0 --end-frame 90
```

Keep the installed Qt/FFmpeg runtime on PATH as with `Run-Cuebase.cmd`. Frame numbers use scene time. The first frame is included, the end frame is excluded, and output names retain their scene frame numbers. Relative image paths in the console input are resolved against the composition file. The exporter refuses a non-empty output directory and writes each PNG atomically. A completed export includes a manifest; an interrupted or failed export may contain completed frames and reports how far it reached.

## Paint reveal

For a shape, text or image, expand Effects. **Reveal** runs from 0 (hidden) to 1 (fully visible). Choose **Linear** for a directional wipe or **Radial** to spread outward from a point. Roughness controls the speckled edge, Softness smooths it, and Direction rotates a linear wipe. Center X/Y moves the radial origin within the object (0 to 1). Animate the numeric controls with the existing diamonds or graph editor. Noise is fixed to the object, so scrubbing backward produces the same edge rather than random flicker. Reveal also masks the object's stroke. Use a separate object when a border must remain visible.

Shapes now offer Rectangle, Ellipse and Triangle under Layout. Triangle provides a native gable/slope shape for broadcast transitions.

Build import and the console renderer resolve relative image texture paths beside the `.cuebuild` file. Keep those image files together when moving a composition. This does not embed media or automatically package other linked assets.

The renderer and controls passed the editor and Studio regression suites. See VERIFICATION.md for results and practical limits.
