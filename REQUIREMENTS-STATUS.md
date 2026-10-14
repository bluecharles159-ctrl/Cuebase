# Cuebase completion ledger

Updated 2026-10-13. This is a development record, not a release certificate.
The user requires the complete requested scope before final delivery. A successful
compile, a prototype, an interface control, or a source archive does not satisfy
that requirement by itself. Do not label a package complete while items below
remain open. Preserve the earlier requests as well as the expansion brief.

## Acceptance process

For each feature: inspect the implementation, exercise the actual workflow,
check undo and save/reopen where applicable, check Build/Studio/export agreement,
and record evidence. Automated coverage and native visual inspection are separate.
External service setup must be verified on the real service before calling it live.
Do not convert an untested feature into a completed item merely because it exists.

## Immediate repair pass

| Request | Current evidence / remaining work |
|---|---|
| Object origins and overlay toggle | Implemented; image-difference check passed within the focused test before its later selection assertion failed. Native visual inspection outstanding. |
| Apply modifier icon and shortcut | Implemented; mesh equality, undo, stack order and Edit Mode rejection assertions passed before a later assertion failed. Full regression rerun pending. |
| Mirror reference object picker | Implemented; viewport pick/cancel assertions passed. Exercise the actual modifier-card button as well. |
| Enter commits and removes input focus | Focused test found direct-spinbox focus retention. Handler added; rebuild and verification pending. |
| Select All feedback / unexpected gizmo movement | Feedback and hidden-object filtering implemented. Test corrected to actually hide camera/light overlays. Rerun pending. |
| Parented Edit Mode coordinates | World-transform fix implemented; assertion was not reached in the failed focused test. Must rerun. |
| Preserve shape appearance when entering Edit Mode | Outline conversion implemented; triangle/shadeless assertion awaits rerun. |
| Rounded cue modal and clean numeric fields | Saving/persistence passed. Captured modal exposes inherited label backgrounds; visual cleanup still required. |
| Loop count and different holds for individual plays | Cue compiler and modal persistence focused tests passed. Studio timing editor still needs direct interaction coverage. |
| Manual cue hold in Build | Continue control and state transition implemented; new checks pending rebuild. |

## Earlier requested workflows

The last committed verification baseline is described in VERIFICATION.md. Its
29 Build and 23 Studio test cases predate the current repair pass.

| Area | Remaining acceptance work |
|---|---|
| Compact UI, icons, rounded checkboxes, dropdowns, help/docs and shortcut layout | Audit all panels against the supplied references, including disabled/focused states and high DPI. Native desktop inspection is outstanding. |
| Background solid/gradient/media, multiple audio sources, text backgrounds and backdrop blur | Existing tests cover several renderer paths; verify parity in Build, Studio and exported content, including borders and alpha. |
| Parenting, isolated-parent transforms, drag parenting, layer search/reveal, origins and percentage corners | Existing coverage is partial; exercise complete interactions, undo and persistence. |
| Graph editor, channel selection, handles, interpolation, key transforms/copy/paste, equal-value spans and persistent navigation | Existing graph regressions are not a complete audit of every requested shortcut and visual state. |
| Screen-element browser, saved-scene switching, crops, playlists, metadata and presets | Existing workflows require a full visual and persistence review. |
| Editable general text/media fields preserving animations | Existing text and media validation coverage; audit naming syntax, labels and all supported object types. |
| Recursive cue boards, duplication, holds/loops, delay scheduling, durations and on-air edit protection | Existing compiler/Studio coverage; verify per-play changes and nested UI, preview highlighting, trim/extension semantics and live edit boundaries together. |
| Studio timeline, removable main-track items, exact clip duration, fixed playhead and navigation | Fixed-center navigation exists; waveform/thumbnail strips and the requested primary-sequence architecture remain open. |
| Replay effects, private scrubbing with live picture-in-picture, return-to-live and emergency content | Partial implementation and regression coverage. Verify full UX, transitions, editable error messages and recovery. Live-camera DVR replay is not implemented. |
| Channel identity, circular imported logos, onboarding, isolated channels and multiple windows | Circular logo treatment exists; production accounts and isolated multi-channel workspaces remain open. |
| Timed multi-source split screens and transition presets | Open. |
| RTMP/SRT, output health and audience dashboard | Local tests exist. Public distribution, real audience-provider data and sustained output/failure recovery remain external acceptance requirements. |
| Google/GitHub/email sign-in and cross-device accounts | Supabase setup documentation exists. Hosted project configuration and real cross-device login are not complete. Never ship mock login as working authentication. |
| Crew collaboration, roles and concurrent content editing | Host-controlled crew workflows exist; native multi-device project/media synchronization and full concurrent editing remain open. |
| Import/export of models, elements, baked cues and animations | Geometry import and PNG sequence output exist. Complete format coverage, portable assets, animation/audio export and round trips remain open. |
| Broadcast-reference study and HGTV reconstruction proof | Only a short editable ident study exists. The requested hour of multi-channel material, full renovation-ad reconstruction, audio and feature-parity demonstration remain incomplete. |
| Installer, GitHub download, website, advertising, trial and monetization | Open. Do not publish or describe a production-ready release before acceptance. |

## Resumed creative expansion brief

Source: the user's pasted “CUEBASE — MAJOR DEVELOPMENT & CREATIVE STUDIO
EXPANSION” document. CREATIVE-STUDIO-PLAN.md records implementation dependencies.

1. **Viewport modes:** audit structural/material/rendered semantics; add adaptive quality.
2. **Composition guides:** persist customizable safe regions, margins, colors, responsive guides and snapping; guarantee exclusion from program/export output.
3. **Texture library:** stable assets, collections, favorites, previews, missing-file repair and portable packaging.
4. **Materials:** ordered layers, blend operations, shared styles, roughness/metal/normal/bump and evaluated controls.
5. **Masks:** shape, alpha, luma, gradient, text, brush, feathering, inversion, animation and combinations.
6. **Text treatments:** reflection, animated light sweep, bevel/extrusion, shadows and glass with shared appearance behavior.
7. **Painting:** nondestructive brush layers, material/mask targets, flow/jitter controls, undo and persistence.
8. **Effects:** editable paths/splines, procedural emitters, particles, fire/smoke and simulation distinction.
9. **Physics:** transformed arbitrary colliders, body types, collision filtering and deterministic caches.
10. **Components:** `::name` registration, quick insertion, source revisions, independent overrides, selective propagation and linked animation/cues.
11. **Docking:** move/tab/float/maximize panels, persist and reset layouts without losing edits.
12. **Broadcast sequence:** actual primary-track content, program/ad segments, cut/ripple/trim, transitions and validation.
13. **Audio:** sample-clock mixing, buses, meters, waveform alignment, effects, automation and quality export.
14. **Devices:** secure pairing, revocation, permissions, offline sync, conflicts, progress and change details.
15. **Team tools:** rich notes, assignments, reminders and schedules.
16. **Templates:** master designs, content fields, full-screen/lower-third/ID variants and shared tokens.
17. **Branding:** original Cuebase application icon after functional work.
18. **Professional feedback:** expand and verify the initial research; distinguish historical resolved issues from current limitations.

## Additional requests received 2026-10-13

- Defer Supabase setup at the user's request; remind them when account integration
  is reached. Continue other work without repeatedly requesting credentials.
- Use the compact Inspector as the shared UI standard: 10px property text,
  small distinct icons, indented child controls and collapsible sections in
  Effects, World, Modifiers and new panels. Remove navy panel backgrounds.
- Retain Current theme, add Darker and Light in Settings > Theme; use very thin
  scrollbars throughout. Theme changes must affect existing and newly opened UI.
- Use an actual licensed icon library with distinct Inspector, View/Align,
  Graph, Animation Cues and Modifier symbols. Lucide subset has been downloaded
  with provenance/license; integration is pending.
- Dropdown option lists, context menus and option-selection dialogs should use
  the supplied object-context-menu reference: rounded padded rows, clear hover
  highlight and consistent nested-menu styling. Form dialogs share the theme.
- Graph displays only selected channels, including multi-selection; clicking a
  visible curve makes it active/bright and dims the other selected curves.
  Rename displayed Position groups to Translation without changing stored keys.
- Canvas zoom must preserve the pointer anchor, keep rulers readable at low
  zoom, and use adaptive ruler/grid spacing. Research Blender navigation.
- User will clarify animation-cue semantics later. Do not invent another cue
  redesign in the meantime; retain and verify the already implemented repairs.
- Reveals: choose mesh/item emitters, fire/sparks/smoke/dust; independent reveal
  styles for pixels, selectable blocks, liquid flow and scattered pieces that
  assemble into the final object. Smooth, constant, stepped and flicker timing.
  Integrate with material/mask/particle systems in the expansion brief.
- Clicking an orange modified key updates/replaces the existing key and makes
  it yellow. Only clicking an unchanged yellow key removes it.
- Text with a background needs independently adjustable text and background
  shadows. Show the text-shadow section when background is enabled.
- Text background auto-fit option in Layout, with X and Y enabled by default;
  either axis can be disabled. Content changes must resize the enabled axes.
- Final website must include Windows/macOS/Linux downloads and documentation.
  Do not advertise operating-system binaries that have not been built/tested.

## Release gates

## Additions received 2026-10-14

- The user's new screenshots supersede the earlier no-blue default request:
  Studio Blue is the new default, while Current, Darker and Light remain presets.
  Compact sizing, indentation and distinct icons still apply. Title bar integrates
  File/Edit/Window/Help and Settings immediately before Minimize.
- Read the complete attached brief a1850c96-13fc-4473-8bcf-92ed36b1697b.
  Earlier direct user instructions override conflicting generated suggestions.
- Production planner: separate virtual preview, selector and a single mixed
  program/ad/element timeline with a moving playhead. Reorderable thumbnail list,
  type filters, drag/drop, context placement and current-item indication.
- Resizable floating/full-screen planner opened from the top right. Drag empty
  board space to create text/image cards; rich-text controls, colors, locking,
  resize handles, empty-card animated dashed border, no overlap with timeline.
- Card connection ports support many-to-one/one-to-many, track moved cards and
  timeline attachments; adjustable solid/dotted/dashed lines. Research Blender
  nodes. Do not promise physical haptics without device support.
- Physics: investigate passive object contact, support, friction and tipping;
  arbitrary colliders must work beyond the existing ground-plane approximation.
- These planner and physics items remain pending implementation/verification.

## Release acceptance

- No known failing regression tests or unresolved data-loss issues.
- Complete request audit with reproducible evidence, not just feature names.
- Native UI review and high-DPI/keyboard/accessibility checks.
- Clean-machine installation and dependency/license checks.
- Public-output, sustained-playout and recovery testing with representative media.
- Auth/service setup and cross-device validation on configured accounts.
- Preserve user projects and provide tested migration/backup behavior.
- Only then produce the requested final downloadable deliverable.
