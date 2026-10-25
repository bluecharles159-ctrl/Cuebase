# Cuebase UI workflow and design requirements

Updated 2026-10-25. This is a design specification, not an implementation claim.

[Editable FigJam flowcharts](https://www.figma.com/board/7YAdr4MUKTRJAIj8zoiyME)

## Source of truth

Read together: Major Development & Creative Studio Expansion; Build Workspace
Development & UI Modernization; the Planning Board brief; and the user's later
corrections. Preserve the existing Build arrangement and project compatibility.
Direct instructions take precedence over wording in generated briefs. In
particular, each cue play has independent holds and timing; curve noise belongs
to the Graph Editor, never inside cue creation.

## Where controls belong

| Surface | Use | Cuebase examples |
| --- | --- | --- |
| Docked workspace | Sustained work with a visible canvas/timeline | Build, Audio, Broadcast |
| Contextual inspector | Continuous edits to the selected item | Transform, Typography, Effects; selected cue details |
| Rounded modal | Bounded setup with a clear outcome | Create cue, import/export, account, channel setup |
| Popover or menu | Short contextual choices | Add item, preset, overflow, alignment |
| Floating board | Extended editing that can maximize | Planner and Text Board |
| Settings modal | App/channel/studio configuration with category navigation | General, Theme, Timeline, Channels, Devices |

Do not dump unrelated fields into a single scrolling panel. Keep a stable header,
grouped body and predictable action footer. Show advanced sections on demand.
Keep frequent live controls reachable without opening a modal. A modal has a
clear title, Cancel/Save or the appropriate action, field-level errors, keyboard
focus containment and restoration. Reopening a cue normally uses its inspector.

## Workspace boundaries

- **Library:** programs, ads, images, video, audio, models and reusable graphics;
  search, filters, thumbnails, grid/list, explicit asset types.
- **Build:** preserve viewport, layers, Inspector and animation editor positions.
  Work / Material / Final change preview quality, not composition. Composition
  guides stay editor-only by default. Materials/textures, paint, masks, effects,
  simulation and text states belong to contextual toolsets, not one flat form.
- **Audio:** tracks, waveforms, buses, meters and processor chains. Audio files
  do not become visible scene objects.
- **Animation:** selected curves and an emphasized active curve; separate
  modifiers and independent animation cues. Text content, scrolling and character
  transitions use the same timeline. Cue creation opens a focused modal; the cue
  list opens a selected-cue inspector, with per-play hold controls.
- **Planning:** resizable/maximizable rounded board; viewport upper-left,
  selector upper-right, sequence/preview below, reorderable list at left.
  One primary sequence uses actual durations; its playhead moves. Notes/image
  cards use connectors and do not cover the timeline. Planning remains off air.
- **Broadcast:** Studio's centered playhead and moving timeline remain distinct
  from Planning. Validate and explicitly hand off a plan. Program segments, ads,
  IDs and interstitials use the primary track; overlays/audio use support tracks.
  Private scrubbing leaves live output running, with a live inset and Return to
  Live. Timed splits and overlays must not obscure output status.
- **Delivery:** export dialog with format, range and destination; progress,
  cancellation and actionable errors; compatible project/media packaging.
- **Studio management:** studio sign-in by default; Editing is an explicit
  alternative. One channel initially; channels isolate content. Additional
  channels/windows, selective shared settings, role-based devices, sync states
  and conflicts are explicit. Supabase activation is still deferred.

## Readability and visual acceptance

Studio Blue remains the default, with existing presets retained. Use shared text,
surface, selection and focus tokens. Primary and secondary enabled text must
reach at least 4.5:1 against their actual backgrounds; aim for 7:1 for compact
labels. Use a readable disabled-text token rather than the border color. Do not
signal states with color alone. Test selected, hovered, focused, disabled and
error states in every theme. Check actual rendered glyphs, not only palette values.

Preserve compact Inspector proportions but expose interface scaling. Use consistent
indents, section spacing and distinct icons with labels/tooltips. Thin scrollbars
must remain operable. Avoid clipped fields, overlapping footers and hover-only
access to essential actions. No UI acceptance based solely on compilation.

## Professional references and adaptation

These are workflow references, not claims that Cuebase already matches them.

- [DaVinci Resolve](https://www.blackmagicdesign.com/products/davinciresolve):
  dedicated task workspaces informed the separation of creation, audio and delivery.
- [Blender viewport shading](https://docs.blender.org/manual/en/dev/editors/3dview/display/shading.html):
  distinct construction/material/rendered views informed Build preview boundaries.
- [vMix interface guide](https://www.vmix.com/help26/GettingStarted.html):
  preview/output separation informed live-output safety. Cuebase's private replay
  and Return to Live behavior are the user's own requirements, not copied behavior.
- [WCAG contrast guidance](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html):
  measurable text contrast informs readability checks; this is not a certification.
- Mobbin: screen-gallery access was unavailable in this session. Inspect actual
  onboarding, settings, modal and asset-browser flows when accessible; do not
  claim private screens or collections have already been studied.

The flowcharts summarize target behavior. COMPLETION-CHECKLIST.md remains the
implementation ledger: creating this design does not complete a software task.
