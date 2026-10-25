# Source review and implementation preparation — 2026-10-15

October 16 follow-up: `rundown-cue-results.txt` now verifies rundown-instance
Continue/Hide resolution and separate on-air UI controls, including copy/source
isolation, reordered slots and deleted sources. The nested media handoff checks
also passed. The findings below describe the original review; replay/seek policy,
audio agreement and shared portable-asset infrastructure remain open.

Completed without compiling, downloading media or changing application code while
the user frees disk space. Findings below are source observations and engineering
recommendations, not claims that the features are fixed. Completion remains
**9 of 80 tasks completed — 71 remaining**.

## Planner graphics: manual playback identity

`ViewportWidget.cpp` and `Playout.cpp` create standalone graphic instances with
IDs `program:<program ID>`. In `AnimationPlayout.cpp`, `triggerElementAnimation`
and `advanceElementCue` resolve normal elements and `placement:` instances only.
Consequently a manual command addressed to a standalone program instance returns
without changing playback. This follows from source inspection; no new runtime
reproduction was run during the disk-space pause.

Required repair for tasks 12/30/49:

- Centralize playback-instance resolution for library elements, placed graphics
  and standalone program graphics. Return the resolved element, instance ID,
  actual start time and media offset together.
- For a program instance, resolve its owning program and current segment; reject
  stale/missing references. Preserve the `program:` identity in cache and commands.
- Wire Continue/Hide to the selected on-air instance, not its library source.
- Test two copies of one graphic: continuing/hiding one must not affect the other,
  its source, another placement, or private preview. Test holds, reordered slots,
  deleted sources, replay and audio agreement.

## Planner media validation

`PlanHandoff::validate` checks the primary file/image and that a custom design is a
JSON object. It does not inspect all media references inside that design. A design
with a missing nested video/texture/audio source can therefore pass this check.

Required repair for tasks 49/52/65: use one resource enumerator for handoff,
portable export and missing-asset repair. Report object names and missing paths;
distinguish optional media from required sources. Validate without mutating the
plan or rundown. Add fixtures for nested missing media, relinking, cancellation,
and successful portable reopening on another directory root.

## Audio timing and monitoring

`NetworkOutput::attachPlayer` appends decoded PCM to a bounded FIFO, and `mix`
consumes samples in arrival order with gain and clipping. These functions do not
use the audio buffer timestamp for timeline placement. This is a source-level gap
for task 40; it does not establish measured drift in an existing recording.

Implement a common sample clock with timestamped source blocks before expanding
the DSP UI. Define seek discontinuities, underruns, latency compensation, channel
mapping and bounded buffering. Build/Studio/record/export must share evaluation.
Keep monitoring independent of program routing; solo must not silently change
the broadcast mix. Add tests with delayed decoding, simultaneous sources, seek,
pause/resume, muted monitoring and device loss before effects chains.

Official reference: vMix documents separate input/master/bus controls, independent
headphone level and solo monitoring that does not affect the mix. These are useful
workflow precedents, not evidence of Cuebase parity.
[vMix Audio Mixer](https://www.vmix.com/help29/Mixer.html)

## Preview versus program

OBS documents an editable preview separate from the live program, with an explicit
transition to air. Cuebase's requested private scrubbing must preserve that
separation. Its **Return to live** action should resynchronize the local monitor;
publishing edited content is a separate operation. This is an engineering
recommendation following the user's requirement that broadcast remain visible.
[OBS Studio Mode](https://obsproject.com/kb/OBS-Studio-Overview#studio-mode)

Acceptance for tasks 34–37: compare program-frame hashes while scrubbing preview;
verify that return animations never reach public output, keyboard focus cannot
seek broadcast accidentally, emergency override remains authoritative, and timed
split layouts use the broadcast clock independently of the preview cursor.

## Docking migration

`MainWindow.cpp` persists three splitter states; `BuildWorkspace.cpp` also uses
splitters. These provide resizing, not the requested complete docking system.
Qt documents nested/tabbed docks and versioned layout save/restore. Give every
panel a stable identity before migration and retain a fallback for old splitter
settings. Validate against the project's installed Qt 6.11.2: the current online
reference labels itself 6.12, so new APIs must not be assumed available.
[Qt QMainWindow](https://doc.qt.io/qt-6/qmainwindow.html)

For task 63, preserve the existing panel instances and their models when docking,
floating or maximizing; do not recreate editors and discard unsaved state. Restore
off-screen windows onto an available display. Acceptance must cover lost monitors,
tabbed/hidden panels, fullscreen restoration, keyboard navigation, an active media
player and layout reset without document changes.

## Next verification sequence after space is available

1. Finish the interrupted one-job application/test/renderer build.
2. Run both trim-test rows, then full Studio and Build suites.
3. Fix and test instance-aware manual commands before signing off planner handoff.
4. Add shared resource validation before portable assets or handoff acceptance.
5. Establish timestamped audio and stable dock-panel identities before dependent UI.

This review advances the listed tasks but does not complete the broad research,
one-hour broadcast study, native UI audit or feature implementation requirements.
