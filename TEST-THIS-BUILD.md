# Testing the Claude recovery checkpoint

Extract the complete Windows ZIP and run **Start Cuebase.cmd**. The source folder
contains the complete current application source, not a patch requiring older
ZIPs. `app/Cuebase.exe` is the runnable application; the other test executables
are included for reproducible diagnostics.

## Animation noise

1. Open Build and key an object's Translation X at two different frames.
2. Select that channel in the graph, then click the noise toolbar icon. You can
   also select keyframes and use **Noise modifier…** from their context menu.
3. Change Strength and Scale, save, and scrub forward and backward. The curve and
   object should follow the same repeatable pattern each time.
4. Select several curves and reopen noise. The channel dropdown edits each
   curve independently. Cancel discards all drafts; Save is one undo step.
5. Reopen the saved composition. Noise should remain. Disable it to compare the
   original animation, or remove it from a curve and save.

See ANIMATION-NOISE.md for the controls and differences from Blender.

## Interaction checks

- Select an object, press Shift+D, then Shift-right-click. The duplicate should
  disappear while the original and earlier edits remain.
- Press G, move outside the viewport, then Escape. The transform should continue
  tracking until cancelled. Native multi-monitor behavior still needs testing.
- Select several objects, then plain-click one of them. Only that object should
  remain selected. Shift-click continues to toggle multi-selection.
- Switch Work, Material and Final. The mode button should keep its width.
- Use Alt+wheel in either Build animation editor: up undoes, down redoes. Ctrl
  temporarily bypasses existing alignment snaps during a canvas move.
- Move an object beyond the camera frame. Work/Material/Wire should show it in
  the darkened surround even with gizmos hidden. Final output stays camera-cropped.

The layer/sub-layer system from the Claude description has not been imported or
implemented in this checkpoint. See CLAUDE-REQUESTS.md and
COMPLETION-CHECKLIST.md for the full remaining work. In particular, do not rely
on Edit Mode to preserve a shape's complete appearance yet; use an independent
copy while testing conversions.
