# Claude transcript recovery — 24 October 2026

The user's pasted conversation is the specification for this recovery. No
Claude-generated source was available to merge. `Cuebase-ClaudeVersion.zip`
was the baseline uploaded to Claude: all 205 compared code/assets/backend/test
files were identical to this repository. It was removed with the user's explicit
permission after recording its hash in artifacts/removed-claude-duplicate.json.
Claude's remote `/mnt/user-data/outputs/Cuebase-Layers.zip` and
`Cuebase-Fixes.zip` were not available on this computer. Their claimed tests are
not Cuebase verification evidence.

## Implemented in this recovery, awaiting current build/test evidence

- Non-destructive per-curve animation noise: context menu, toolbar, independent
  settings, deterministic evaluation, saved tracks, undo and export integration.
- Duplicate always takes an independent undo snapshot. Modal cancellation takes
  priority over Shift-right-click placement.
- Keyboard transforms grab/release the mouse while active.
- Plain canvas click selects only the clicked object; Shift retains explicit
  multi-selection behavior.
- The view-mode toolbar button keeps a constant width.
- Alt-scroll traverses undo/redo in both Build animation editors.
- Ctrl temporarily bypasses alignment snapping, as Alt already did.
- Work/Material/Wire views show off-camera objects under a dark translucent
  overlay. Final rendering remains cropped to its camera.

## Still required from the transcript

- Layer/front-middle-back stack model, 14-pixel G/Z steps, delayed/pulsing NEW
  layer at both ends, release commit, cancel restoration, colors and overlap-only
  broadcast rendering. Existing overlay Z-index controls do not satisfy this.
- Dual-camera layout, side-view toggle and final-camera aspect presets.
- Boolean effects for text.
- Full grid snapping (the current bypass applies to existing alignment snaps).
- Shortcut reference, loading transitions and requested timeline redesign.
- Multi-object handle audit and native mouse verification beyond viewport edges.
- Shape-to-mesh Edit Mode preserves some properties in memory but hides surface
  panels and does not preserve their full rendering. This needs a coordinated
  renderer/Inspector fix, not simply making controls visible.

Markdown text and image/video element backgrounds already exist; they were not
newly copied from Claude. Existing keyframe copy/paste and apply transforms still
need the user's requested broader audit. Supabase and cloud archive remain
deferred. Mobbin was connected in Claude, not in this Codex task; no Mobbin access
or review is claimed here.

The Windows ZIP is a complete source/runtime test checkpoint for the current
implementation. It is not completion of the full creative-studio backlog or a
verified Windows/macOS/Linux public release.
