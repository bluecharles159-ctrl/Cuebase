# Reveal effects in Build

Select a flat graphic, image or text object and expand **Effects**. Animate
**Reveal** from `0` (hidden) to `1` (complete). Switch the viewport to Material or
Final to inspect finishing effects; Work deliberately shows the plain object.

| Style | Appearance |
|---|---|
| Wipe | Directional or radial reveal with roughness and softness controls |
| Pixels | Individual texture pixels appear in a seeded order |
| Blocks | Blocks appear in a seeded order; Block size sets their size |
| Liquid | A rising, wavy reveal front |
| Assemble | Rotated and displaced image tiles settle into their final positions |

Scatter distance affects Assemble. Reveal seed makes the spatial arrangement
repeatable. These are two-dimensional surface effects; the Liquid style is not a
fluid simulation and Assemble does not create editable 3D mesh fragments.

**Reveal timing** remaps the animated progress: Linear, Smooth, Constant, Steps or
Flicker. Timing steps controls Steps. The animation graph still controls the
underlying keyframe interpolation. The reveal is evaluated from the current
progress and seed, so scrubbing backward and exporting do not accumulate a
separate simulation state.

The effect and its keys are saved with the object. PNG sequence export uses the
same surface renderer. The acceptance tests exercise style/timing controls,
undo/redo, saved settings and exported frames; results are recorded in
VERIFICATION.md. Mesh emitters, particle simulations and volumetric fire/smoke
remain separate work.
