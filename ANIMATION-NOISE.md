# Animation noise

Select keyframes in either animation editor, right-click and choose **Noise
modifier…**. The noise toolbar button also accepts selected channels when no
keys are selected. Noise applies to each entire selected curve, including its
constant extrapolation before and after its keys. Locked objects are skipped.

The rounded editor lists the targeted curves. Each has independent settings;
switching curves keeps its draft. Save commits the drafts as one undo step.
Cancel changes nothing. Remove noise removes only the current curve's modifier
when saved. Uncheck Enabled to retain settings while bypassing the effect.

- Scale controls the time spacing of fluctuations, in seconds.
- Strength controls their magnitude in the property's own units.
- Offset shifts the pattern in time. Seed chooses a repeatable pattern.
- Detail levels add higher frequencies; Roughness and Lacunarity shape them.
- Replace adds centered noise; Add/Subtract use positive noise; Multiply scales
  the underlying curve by noise. Influence blends with the original curve.
- Restrict time range limits the effect and optionally fades it in and out.

The workflow follows the [Blender F-Curve noise controls](https://docs.blender.org/manual/en/5.1/editors/graph_editor/fcurves/modifiers.html).
Cuebase uses its own deterministic smooth value-noise implementation, not
Blender's exact noise sequence. Time controls use seconds rather than frames.
This version supports one noise modifier per curve, up to eight detail levels.
It does not implement Blender's complete modifier stack or expression drivers.

The graph draws the evaluated noisy curve while key points/handles continue to
show the original editable curve. Noise is stored with animation tracks, so
scene saves, component animation links, undo/redo and the shared output renderer
retain it. Random-access evaluation makes forward playback and backward seeking
agree; it does not depend on the order frames are rendered.
