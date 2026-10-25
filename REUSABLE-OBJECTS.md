# Reusable objects

Implementation checkpoint for expansion brief section 8. Verification results are
recorded in VERIFICATION.md; this document is not a release certification.

## Create and insert

In Build, rename an unlocked mesh, shape, text or image object `::divider`.
Cuebase stores a source definition with a stable ID, removes the prefix from the
display name and shows a purple component icon in Layers. Names are labels, never
the identity used for linking. Objects with modifiers referencing another object
must have those modifiers applied before registration.

Press `/` while the canvas has focus in Object Mode, or use Components in the
Build toolbar. Search filters immediately. The browser includes rendered
thumbnails, categories, recently used ordering and editable library metadata.
Select a result and choose Insert linked instance, or double-click it. The browser
stays open for repeated insertion and scene selection.

Definitions persist in the current production library. Each new instance receives
its own object ID and cue IDs, with animation and cue links enabled by default.
The source's published link preferences become the defaults for later insertions.
Geometry, materials, animation, cues, effects and appearance have separate links.

## Source changes and local edits

Editing an instance changes only that instance. The browser identifies its source
revision and locally overridden groups. Placement, text content and image content
remain local when source changes are applied.

Placement here means the object's base transform. Linked transform keys still
control their animated channels during playback; this does not add a separate
instance-offset transform above the animation. Longer copied tracks extend the
scene duration even when cue linking is disabled.

Select the registered source and choose Publish selected source. This saves a new
library revision without modifying existing instances. Select instances, then use
Review source changes to choose the groups to update. The review shows source and
instance revisions and locally overridden groups. Preserve local overrides is on
by default. Turn it off only to deliberately replace local values in the selected,
linked groups. Unchecked links always retain their local values.

The review refuses to apply if the scene, selection or library changed since it
opened. Scene updates, instance insertion and link edits support Undo/Redo.
Library publication and metadata saves persist independently of scene undo; undoing
a scene does not erase a published library revision.

Failed library saves leave both the scene and in-memory library unchanged. Missing
source media prevents insertion or propagation. Duplicating an instance preserves
its local override detection and creates independent cue IDs. Removing that object
removes its copied cues; undo restores both.

A duplicate of a source with unpublished changes also keeps the last published
baseline, so those changes are recognized as local overrides. Self-referencing
modifiers are remapped to the new object without creating a false override.

## Scope

- Components currently contain individual renderable objects. Parent/group
  assemblies and external modifier dependency graphs are not captured. Source
  geometry and local transforms are copied without its external parent.
- Updates apply to selected instances in the open editable scene. Open another
  scene to review its updates. Saved scenes and on-air snapshots are not silently
  rewritten; a bulk cross-document propagation interface remains separate work.
- Cue definitions capture the source scene's unowned frame-range cues. Copied
  ranges use Cuebase's scene-time playback semantics. They are not independent
  per-object animation clocks or baked animation exports.
- Referenced media remains linked by path. This is not a portable component bundle;
  use composition media management for missing assets and portable compositions.
- A source object removed from its scene leaves its last published definition
  available for insertion. Reassigning source ownership and deleting library
  records are not provided by this checkpoint.
