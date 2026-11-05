# Cuebase

**Create scenes. Shape motion. Plan your broadcast.**

Cuebase is a desktop production workspace combining visual scene creation,
animation, audio assets and broadcast planning.

> **Development preview:** substantial features are still in progress. This
> build is for evaluation and feedback, not unattended production broadcasting.

## Windows download

The verified preview candidate is commit `02b1f32`.
[Open its successful build and downloads](https://github.com/bluecharles159-ctrl/Cuebase/actions/runs/41978291483).
Select the **Cuebase-Windows** artifact. GitHub may require sign-in for Actions
downloads. A public Releases download has not yet been published.

1. Download and extract the artifact ZIP, then extract the application ZIP inside.
2. Keep the complete extracted folder, including DLLs and plugins.
3. Double-click `Start Cuebase.cmd`. Save and close Cuebase before replacing an older build.

Only Windows is packaged at this checkpoint. No signed installer, macOS build or
Linux build is claimed.

## Explore the preview

### Build and Form Animation

Create scenes, select vertices/edges/faces, and confirm mesh transforms or
extrusions to capture form changes. Open the key icon beside Form Animation to
adjust **Influence**: zero restores the captured start; one reaches the authored
result. Insert/remove keys with the panel buttons, or hover Influence and press
**I** or **K** to insert a key.

Form controls follow the playhead and support rename and bake/remove with undo.
Supported mesh operations transfer earlier forms to their new topology; an
incompatible transfer is rejected. See [Form Animation](FORM-ANIMATION.md) for
controls, supported operations and the remaining surface limitations.

The Final picture stays visible beside the work viewport. Switch the work camera
between Broadcast, Front, Right and Top without altering output. The format
button above Final opens saved HD, UHD, portrait, square and custom presets.

### Audio

Import sounds into the shared library, audition source audio, and place instances
in Build scenes or on the Studio timeline. Controls include speed, volume,
source offset, scene start, fades and scene mute.

**Audio effects** provides ordered, bypassable echo, EQ, high/low-pass filters,
compression, limiting and gain. Rendering creates a separate WAV and imports it
into the library. This is offline processing, not a live DSP mixer.

### Layers and production planning

Use object Z values and forward/backward actions to control overlap without
changing object size. The project also includes program scheduling, cue boards,
preview workflows and a production planning board; consult the backlog for the
parts still under development.

## Build from source

Use the `default` CMake preset with the MSYS2 UCRT64 toolchain. Dependencies and
the reproducible Windows build steps are in
[the workflow](.github/workflows/windows-build.yml).

```sh
cmake --preset default
cmake --build --preset default
```

Close a running Cuebase instance first: Windows locks its executable while it is
running. Native Visual C++ is not the configured compiler for this preset.

## Quality and feedback

- [Verification record](VERIFICATION.md)
- [Feature acceptance checklist](COMPLETION-CHECKLIST.md)
- [Preview release notes](RELEASE-NOTES-PREVIEW.md)

When reporting a bug, include the commit/build, Windows version, reproduction
steps and a minimal example. Remove credentials and private media before sharing.

Company branding is pending the project owner's selection. This repository does
not claim affiliation with existing businesses named Studio 11.
