# UI, text and reveal verification pass — 2026-10-13

This is a development checkpoint, not the final release audit.

## Implemented and checked by the 2026-10-14 regression run

- Shared Studio Blue (new default), Current/Darker/Light palettes, compact typography, narrow scrollbars,
  Inspector-style section headers, and option-list/menu selection styling.
- Bundled Lucide icons with pinned provenance and licenses. Qt Svg is a build
  dependency; assets are embedded in the binary and included in source packaging.
- Graph curves restricted to selected channels, with an active curve; displayed
  position groups are named Translation. Stored property names remain compatible.
- Adaptive canvas ruler intervals and pointer-anchored navigation.
- Orange modified keys update their value before a subsequent click removes them.
- Text Layout auto-fit has independent X/Y switches and padding. Evaluation and
  rendering use the same text measurement, including Markdown and font settings.
- A separate glyph shadow is available when a text background is enabled.
- Surface reveal styles: Wipe, Pixels, Blocks, Liquid, Assemble. Timing options:
  Linear, Smooth, Constant, Steps, Flicker. Sampling is deterministic in object
  space, including block seed and fragment offsets.

Mesh/particle reveal sources (dust, smoke, fire/sparks and selected mesh emitters)
are still pending. The surface liquid reveal is a procedural mask, not a fluid
simulation. Fragment assembly currently operates on the rendered surface, not
detached volumetric mesh geometry. These distinctions must remain clear in the
final capability audit.

The latest screenshot reference supersedes the earlier neutral-only default.
The integrated title bar includes Settings directly before Minimize. Both CTest
suites pass (36 Build cases / 23 Studio cases); see VERIFICATION.md for limits.

## References

Blender's official Graph Editor documentation describes channel-based curve
management and isolation; this informs the selection model rather than claiming
identical behavior in every shortcut:
https://docs.blender.org/manual/en/4.5/editors/graph_editor/introduction.html
https://docs.blender.org/manual/en/3.6/editors/graph_editor/channels/introduction.html

The search index exposed relevant excerpts, but the full manual pages returned
HTTP 402 through the research browser during this pass. Full navigation reference
review remains pending; pointer anchoring is checked directly against coordinates.

Qt stylesheet support: https://doc.qt.io/qt-6/stylesheet-reference.html
Lucide license: https://lucide.dev/license
