# Build viewport appearance

Use the labeled view-mode menu at the left of the Build viewport toolbar.
Press **Z** while the canvas has focus to cycle Work → Material → Final → Wireframe.
The choice is remembered locally and does not become part of the composition.
The initial mode is Work.

- **Work:** neutral gray surfaces, neutral mesh lighting and readable light text.
  Images become plain shapes. Text size, wrapping, alignment, object transforms,
  opacity, corners, visibility and clipping remain available for layout. Finishing
  colors, gradients, shadows, backdrop blur and reveal effects are suppressed in
  this preview. The source appearance and animation remain unchanged.
- **Material:** authored flat-surface styling and images, with neutral viewing
  light on meshes. This is useful for examining surfaces independently of the
  scene's lights. It does not replace a physically based material renderer.
- **Final:** the existing output renderer, including authored lighting and effects.
  Editor overlays still follow the separate overlay controls.
- **Wireframe:** mesh edges for inspecting geometry; flat graphics retain a plain
  structural preview.

Studio output and export do not read this editor preference. Guides, selection
handles and the gray working appearance are editor-only. Work intentionally hides
finishing effects, so check Final before judging a reveal, shadow or material.

The adjacent quality menu offers **Auto**, **Full**, **Half** and **Quarter**.
Auto uses half resolution during playback and modal transforms and full resolution
when idle. All modes limit their image buffers to 2,073,600 pixels (1080p pixel
count), including at high zoom. The limit is for the editor preview, not export.
Guides and editing handles are drawn afterward at widget resolution.

Build previews render into an image buffer so backdrop effects can sample earlier
surfaces. Final, PNG stills and PNG sequences use the project's motion-blur
settings. The sequence shutter interval follows the exported frame rate.
The 53-case editor suite verifies blurred animation export and exact
Solid/Gradient/Media preview-to-PNG comparisons. Measured performance budgets and complete Build/Studio/export
lighting acceptance remain part of checklist item 50.

Final and the clean preview also apply the project's display view, contrast look,
exposure and gamma after rendering, matching the transform used by Studio and
PNG export. Work and Material stay ungraded. The 54-case editor suite verifies
the view/look combinations, clean-preview color, unchanged Work appearance and
alpha preservation. See COLOR-MANAGEMENT.md for scope and compositing details.

The subsequent 55-case suite verifies Point, Sun, Spot and Area lighting across
Work/Material/Final, including parent-hidden lights, light-color changes and animated
light-power export. The proof sheet is `artifacts/build-lighting-modes.png`. Native
interaction review and measured performance budgets remain open.
