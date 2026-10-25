# Composition guides in Build

Open **View / Align → Composition guides…** to set title-safe and action-safe
insets, grid spacing and the custom-guide color. Insets are percentages on each
edge. Grid spacing is expressed in composition pixels; at small zoom levels the
display spacing is limited so grid lines do not crowd together.

Drag from the left ruler to create a vertical guide, or from the top ruler for a
horizontal guide. Drag a guide to reposition it; drag outside the composition to
remove it. **Clear guides** removes the custom guides together. Guide changes and
settings participate in the composition's undo/redo history.

Custom guides are saved as normalized positions in the composition, so changing
output resolution preserves their placement. Save to Screen Elements to persist
the composition. Reopening a different scene loads that scene's guides.

The existing Snap control includes the custom guides. Alt temporarily bypasses
snapping. Guide lines and safe-area overlays are editor-only and never appear in
Studio output or exported frames. Hiding all overlays also disables guide picking
so an invisible guide does not intercept object selection.

Implementation acceptance is recorded in VERIFICATION.md. This addition does not
claim automatic responsive object-layout constraints or full native UI acceptance.
