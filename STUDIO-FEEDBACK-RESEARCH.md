# Production software feedback and UI research — updated 2026-10-15

This is a small qualitative review of publicly accessible discussions, not a survey or a claim about the frequency of problems across the industry. Some reports concern old releases or resolved bugs. The observations below distinguish reported experience from Cuebase engineering recommendations. No interviews or broadcast-station trials were conducted.

## Research deliverable and evidence boundaries

This report completes the scoped product/UI research and sourced feedback report
(checklist task 74). Implementation, native UI review, the one-hour audiovisual
study and HGTV reconstruction are separate tasks and remain incomplete. The older
feedback below retains its original qualification; the new source review is in
LOW-DISK-REVIEW.md. Search snippets alone do not establish current product defects.

## What established production tools provide

| Verified reference | Useful workflow | Cuebase implementation decision |
|---|---|---|
| [OBS Studio Mode](https://obsproject.com/kb/OBS-Studio-Overview#studio-mode) | Separate editable preview and live program, with an explicit transition | Keep preview scrubbing local. Return to live resynchronizes the monitor; taking edited content to air is a different command. |
| [vMix Layers](https://www.vmix.com/help29/InputSettingsMultiView.html) | Position, crop, border, visibility, ordering and reusable multi-input layouts | Use stable source IDs and reusable layouts; animate geometry between layouts without restarting source playback. |
| [vMix Audio Mixer](https://www.vmix.com/help29/Mixer.html) | Input/master/bus strips, separate headphone level and solo monitoring | Clearly separate monitor and program routes; meters must measure actual audio at identified processing stages. |
| [Qt main-window layout facilities](https://doc.qt.io/qt-6/qmainwindow.html) | Nested/tabbed dock panels and stored window layout | Migrate splitters using stable panel identities; retain existing editors and document state while panels move. Verify APIs against installed Qt 6.11.2. |

These are documented capabilities, not measured comparisons of reliability or speed.

## Visual design systems: primary creator evidence

- [HGTV / Nathaniel Howe Studios](https://nathanielhowe.com/projects/2023_hgtv_refresh/prj_index.html)
  describes consistent typography/grids alongside variable textures and a recurring
  gable motif. Recommendation: separate reusable layout, content fields, materials
  and motion; permit show-specific appearances without rebuilding the composition.
- [Disney Jr. / Makiné](https://www.makinestudios.com/case-study/disney-jr)
  describes a modular graphical system spanning platforms and seasonal packages.
  Recommendation: master designs with aspect-ratio/content variants and explicit
  override controls, rather than detached copies with matching names.
- [Nickelodeon / Shalinder Matharu](https://www.shalindermatharu.com/work/nickelodeon)
  identifies bumpers, tags and logo bugs, and separates final on-air work from
  exploratory work. Recommendation: label graphic purpose and distinguish a
  preview/draft from an approved production asset.
- [Studio Universal / Luke Tilly](https://www.luketilly.com/work/studio-universal-global-identity)
  describes cinema/poster influences and cinematic typography. Recommendation:
  adjustable headline hierarchy, safe regions and footage-aware layouts.
- [Discovery / Stephen Royle](https://stephenjohnroyle.com/branding)
  describes genre palettes, territorial adaptations and a Discovery Science
  refresh designed for multiple screen contexts. Recommendation: shared brand
  tokens with language/territory variants and text-fit checks.
- [E! Style Collective / Jon Berry](https://jonberrydesign.com/estylecollective)
  describes a flexible package for online and on-air use with multiple palettes.
  Recommendation: editable graphic variants and portable material/style presets.

These descriptions were read; their embedded videos were not watched in this
pass. No extra audiovisual viewing time is claimed. An M-Net Movies Zone concept
pitch was located, but a pitch does not prove that its graphics aired:
[creator's concept](https://www.behance.net/gallery/96149671/M-Net-Movies-Re-Brand-Concept).

## UI decisions for Cuebase

The user's Studio Blue screenshots remain authoritative for color and sizing.
References inform behavior; they do not replace the requested compact Inspector.

1. Use the same property-row height, indentation, collapsible headers and input
   behavior across Effects, World, Modifiers and future mixer/layout panels.
2. Show purpose and state with text plus distinct icons. Preview, Program,
   Draft, On air and Standby must not rely solely on color.
3. Keep common operations directly accessible: add source, choose layout,
   duration, preview, apply. Put advanced crop/routing/interpolation controls in
   expandable sections instead of increasing every panel's density.
4. Keep transition animations brief and local to UI where appropriate. The
   return-to-live graphic must never contaminate the actual program output.
5. Expose audio routing and monitor state beside controls, not in tooltips alone.
   Wheel adjustments should apply only to the intended hovered/focused control.
6. Keep source content separate from presentation. A layout switch must not
   restart a camera/video or silently change which microphones are on air.

## Current gap and priority update

| Priority | Current evidence | Next implementation / acceptance | Difficulty |
|---|---|---|---|
| Critical | Program-instance manual cue IDs are not resolved by Continue/Hide | Shared instance resolver; independent-copy command/audio tests | Medium |
| Critical | Network audio consumes arrival-order PCM rather than timestamped blocks | Sample-clock scheduler, discontinuity handling and independent monitor buses | High |
| High | Handoff validates top-level media but not nested design dependencies | Shared asset inventory, relink report and isolated package reopen | Medium/high |
| High | Timeline previews now pass; trim test update and app build await disk space | Finish build, both trim layouts and full regressions | Low/medium |
| High | Timed multi-source split-screen system remains open | Stable layout slots, broadcast-time events, five transition presets and audio policy | High |
| Medium | Splitters exist; full docking does not | Stable dock identities, migration, tab/float/reset and lost-monitor recovery | Medium/high |
| High | Hosted authentication is deferred by the user | Resume real service setup when requested; do not count local profiles as live accounts | High |

Priorities and difficulty are engineering judgments, not survey results or fixed
delivery estimates. Passing local streaming tests does not establish sustained
or public-network reliability. Full implementation acceptance still follows
REQUIREMENTS-STATUS.md and COMPLETION-CHECKLIST.md.

## Observed feedback

### Predictable playback under memory pressure

Adobe's After Effects team described a fix for preview frames being purged and re-rendered while the cache indicator could still imply they were present. The explanation identifies memory pressure and which frames the cache retained as relevant factors. This is a documented historical fix, not evidence that current After Effects retains that bug. [Adobe's cache fix explanation](https://community.adobe.com/questions-534/fix-for-cached-frames-being-re-rendered-during-preview-314143).

**Cuebase recommendation:** use bounded caches keyed by scene revision, source media revision and frame time. Display cache status from the actual cache. Prioritize the next playback frames and cancel obsolete background work when a user edits. Difficulty: high; priority: high on this 4 GB development machine.

### Reliable audio monitoring and understandable routing

An OBS user reported monitor delay accumulating over time. A collaborator linked an earlier investigation and identified low-latency buffering as a possible cause; the reporter said the suggestions resolved the issue and closed it. The report concerns OBS 24–26, not a verified current regression. [OBS issue 3577](https://github.com/obsproject/obs-studio/issues/3577).

**Cuebase recommendation:** distinguish preview, headphones and program buses explicitly. Test the encoded output and the monitor independently over long runs, including device changes and underruns. Short localhost streaming tests are useful but insufficient for drift assessment. Difficulty: high; priority: critical before public broadcast release.

### Assets are valuable when reuse and overrides are understandable

In a Blender user discussion, several participants praised the asset browser for everyday reuse, while others struggled to discover it, save assets or distinguish linked instances from editable copies. Replies pointed to library overrides, and users disagreed about append behavior. That disagreement is evidence of discoverability problems in this sample, not proof that Blender lacks the capability. [Blender asset-browser discussion](https://www.reddit.com/r/blender/comments/1et4en4/how_many_of_yall_use_the_asset_browser_i_really/).

**Cuebase recommendation:** label Source, Linked instance and Independent copy explicitly. Show which properties are overridden, offer a preview of source propagation and provide one-click registration/search. The `::` name shortcut should invoke the same registration action as the visible command. Difficulty: high; priority: high for repeated broadcast graphics.

### Moving a project must preserve its dependencies

A Ross XPression user described image paths breaking when moving packages between machines and uncertainty about omitted assets. Ross staff explained relative-path expectations and that packages include linked assets rather than every unused file in a project folder. This highlights the need to distinguish referenced and unreferenced dependencies. [Ross export/deploy discussion](https://rossvideo.community/discussion/trouble-with-transporting-a-project-between-two-different-machines-exportdeploy).

**Cuebase recommendation:** export a dependency manifest, show missing assets before packaging, preserve relative paths and let users include additional files used indirectly. Test the package in an isolated directory without access to original assets. Cuebase's relative texture import is only one part of this workflow. Difficulty: medium/high; priority: high before team distribution.

### Precise metering and practical controls

An older vMix development discussion includes a request for a decibel scale on volume meters and more explanatory documentation. It is historical feedback rather than a current missing-feature claim. The contemporary vMix user guide describes pre/post meters and separate headphone routing. [vMix discussion](https://forums.vmix.com/posts/m7641-vMix-12-Coming-Soon), [vMix 28 guide](https://forum.vmix.com/help28/vMixUserGuide.pdf).

**Cuebase recommendation:** use labeled dBFS meters with peak hold, clipping indicators and clearly identified pre/post taps. Base readings on processed audio samples, not a cosmetic animation. Difficulty: medium for metering, high for the bus/DSP system; priority: high.

### Waveform visibility needs its own reliability checks

Search results from Blackmagic's forum describe historical waveform visibility issues with stacked timelines and linked audio. Direct page access failed in this session, so this finding is provisional and excluded from any claim of fully reviewed discussions. [Stacked waveform report](https://forum.blackmagicdesign.com/viewtopic.php?p=508255&t=91050), [linked-audio report](https://forum.blackmagicdesign.com/viewtopic.php?f=38&t=202291).

**Cuebase recommendation:** verify waveform generation, cache invalidation and source-offset mapping independently from audio playback. Zooming or trimming must not change what source samples a clip represents. Difficulty: medium/high; priority: high for the requested timeline.

## Recommended Cuebase order

| Priority | Improvement | Why / implementation direction | Difficulty |
|---|---|---|---|
| Critical | Stable live output and audio clocks | Soak tests, underrun diagnostics, bounded queues, safe standby and explicit preview/program routing | High |
| High | Reliable editing transactions | Consistent Enter/cancel/undo, correct parent coordinates, visible mode and selection feedback | Medium |
| High | Portable assets | Asset IDs, dependency manifest, relink and isolated package validation | Medium/high |
| High | Time-correct timeline | Shared source mapping for waveform/thumbnail/trim; undoable split/ripple; exact duration validation | High |
| High | Audio mixer and meters | Sample-clock buses and automation first, then processors and presets | High |
| High | Reusable broadcast components | Stable source revisions, independent copies, explicit links and local overrides | High |
| Medium | Workspace docking and guides | Persistent accessible panel layouts, maximize/restore, clean output overlays | Medium/high |
| Medium | Advanced appearance and simulations | Shared renderer contracts, deterministic caches and measurable resource budgets | Very high |
| Medium | Team sync and notes | Real account provisioning, revocable devices, conflict handling and media transfer | Very high |

These priorities are engineering judgments informed by the sources and the Cuebase code, not rankings voted on by production professionals. Current Cuebase still lacks the full audio/DSP, portable-asset, hosted-account, simulation and docking systems described here. The complete channel-reference study and renovation-ad recreation remain separate unfinished work.
