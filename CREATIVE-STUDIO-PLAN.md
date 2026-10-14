# Creative Studio expansion — 2026-10-12

The user has resumed the previously held expansion brief. This document records the architecture and delivery order; unchecked work is not implemented functionality.

On 2026-10-13 the user explicitly required full requested scope before final
delivery. REQUIREMENTS-STATUS.md is the completion ledger. Intermediate working
builds are development checkpoints, not the requested final package.

## Existing architecture and immediate repair pass

- `ElementDesign` stores objects with stable IDs, parents, modifiers, property tracks and extensible JSON style fields. Its CPU renderer serves Build, Studio and frame export. Changes to scene semantics must remain consistent across all three.
- `BuildModel` owns selection, edit operations and snapshot undo. `BuildCanvas` draws editor overlays and handles transforms. Parent transforms must be used for both visible geometry and edit vertices.
- `PropertyInspector` stores expansion per object. Shared numeric widgets live in `UiControls`; committing a typed value must end input focus without starting an unrelated action.
- `CueSequence` compiles authored cues and independent board snapshots into playback steps. Per-play hold settings belong in this compiler, not just in widgets. Existing cues without a `plays` array retain their prior behavior.
- Build and Studio currently use splitters, not a complete dock manager. Studio splitter state persists. Native floating/tabbed docking remains work to implement.
- Audio currently uses linked sources, timing/fades/rate controls and Qt media players. This is not an effect-chain mixer or a professional DSP engine.
- Network output has local receiver regression coverage. That does not establish long-duration or public broadcast reliability.

The first repair pass addresses committed input focus, explicit selection feedback, correct parented Edit Mode coordinates, preserved shape conversion, optional origin dots, modifier application and mirror picking, and a rounded cue modal with individually configurable holds. Build and Studio must agree on cue duration and playback boundaries.

## Reference interpretation

The supplied HGTV images distinguish a grey composition scaffold (safe regions, alignment, headline hierarchy) from the finished textured or photographic graphic. Cuebase should retain one composition while changing editor display modes. The four graphics also motivate a master design with content and layout variations. They are workflow references, not bundled logos, textures or templates.

## Delivery phases

1. **Interaction and document foundations.** Finish the repair pass with undo, reopen and playback tests. Introduce versioned scene/component/material records, stable references, migration fixtures and validated command transactions before adding more cross-scene dependencies.
2. **Workspace shell.** Give panels stable IDs, then implement dock/tab/floating containers, maximize/restore, layout presets and reset. Preserve focused editors and unsaved data when reparenting panels; store local workspace state separately from shared project data.
3. **Viewport and guides.** Separate structural, material and rendered quality policies. Persist guides in composition coordinates with output-independent margins, configurable colors and snapping. Keep all editor overlays out of program output and export. Extend current guides rather than inventing a second coordinate system.
4. **Assets and appearance.** Add project asset IDs, missing-file resolution, bounded asynchronous thumbnail caches, collections/favorites and portable manifests. Build ordered material/texture and mask stacks with serialized blend operations; expose only properties that the renderer evaluates. Add light sweep, text treatments and nondestructive brush layers on this foundation.
5. **Components and graphic variants.** Register `::name` as a component with a stable ID and colored icon. Store source revision, link policies (animation/cues enabled by default) and local overrides separately. Propagation must show a change preview and preserve local content. Build searchable insertion and shared design tokens before master-template variations.
6. **Effects and simulation.** Represent paths as editable splines. Distinguish path-driven procedural particles from physical solvers. Colliders use world transforms; implement broad/narrow-phase collision, filtering and deterministic caches before promising arbitrary rigid-body simulation. Account for 2D/2.5D geometry and project frame rate.
7. **Broadcast timeline and audio.** Maintain one actual on-air sequence with linked program segments and explicit ad/promotional content. Use shared source-time mapping for clips, waveform bins and thumbnails. Add undoable split/ripple/trim and validation. Establish a sample-clock mixer, buses, automation and metering before adding EQ/dynamics/time effects, sidechain and pitch/time processing.
8. **Accounts and collaboration.** Provision hosted authentication before claiming cross-device accounts. Use device-specific revocable tokens, expiring pairing challenges and authorized approval; separate media transfer from document sync. Add revision conflict handling, offline queues, activity summaries and local/shared setting boundaries. No permanent shared pairing secret.
9. **Production tools and finish.** Build shared rich notes, assignments and reminders over authenticated project membership. Add original template presets and application branding, then package, run long-duration output/failure tests and update the professional-feedback report.

## Compatibility and acceptance rules

- Keep existing project loading and current rendering behavior unless a migration explicitly changes it. Never use names as object identity.
- New appearance, audio or effect controls require an implemented evaluation path, persistence and undo. No decorative controls that imply unavailable behavior.
- Asset copying/export requires a manifest and missing-resource report. Do not include user credentials or reference media in application packages.
- Source components, instance overrides, authored cues and placed board snapshots are distinct data. Duplicated broadcast placements remain independent.
- Validate interactions at boundaries: exact frame/hold transitions, parent transforms, cancel/undo, missing sources, interrupted exports, stale edits and output stalls.
- This is a phased program of work, not a statement that all requested capabilities already exist. Hosted credentials, the full renovation-promo recreation, one-hour reference study, advanced audio, simulations and full docking remain outstanding.
