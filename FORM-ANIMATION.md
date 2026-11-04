# Form Animation

Form Animation captures changes to mesh vertices, edges and faces as independently
keyable influences. It is available from **Build → Form Animation** or **Tab**.
Select vertices, edges or faces with **1**, **2** or **3**. Move, rotate, scale,
extrude or inset, then confirm to capture a form. Escape or right-click cancels
the operation and its topology changes.

Open the key icon beside the mode control to choose and name a form. Influence
**0** removes that form's captured displacement; **1** applies it fully. With
several forms, each influence contributes independently to the authored mesh.
The panel follows the playhead. Hover Influence and press **I** or **K** to insert
or replace a key. The explicit Remove current key button removes even a modified
key; Bake and remove form keeps its current appearance and removes its track.
These changes support undo.

Later extrusion, inset, subdivision, loop cuts, fill, flatten, merge, deletion,
normal flipping, face splitting/separation and loose-part separation transfer
existing form offsets to their new vertex indices. Joining meshes transfers
their form curves into the active object's coordinates; conflicting form IDs
are remapped so independently animated parts keep their own controls. Applying a
modifier prefix evaluates the authored full mesh and each zero pose, preserving
influence keys rather than applying them twice. If an operation cannot preserve
matching topology, it is rejected instead of silently detaching an old form.

Editing at partial influence uses the displayed positions for movement, scaling,
rotation and inset centers. Existing form tracks and topology survive composition
save/reopen. The same evaluator feeds Build, Studio and PNG rendering.

Text, images and shapes retain their source appearance and relevant Inspector
controls when converted to editable surfaces. Entering the mode preserves output
pixels, including padded shadows. Convex planar four-corner forms warp the full
padded texture, retaining outward shadow and blur during deformation. Other
deformed surfaces map content to individual faces; outward effects on those
faces and combined surface materials remain part of the open surface workflow.
These features are not complete 3D material support.

## Try the editable example

The Windows package includes `examples/FormAnimation.cuebuild`. Drag it into
Build, select **Animated form**, and press Play. Its top face rises using the
keyed **Lift top face** influence. `form-start.png` and `form-end.png` show exported
frames at zero and full influence. Open the form controls to adjust or rekey it.

## Verification

GitHub run **41922990132**, commit **cbb2b4e**, passes seven suites, 29 native
creative cases and 25 native editor cases. Packaged creative, editor and component
checks pass without a development toolchain on PATH. The tests include origin
changes, text resizing, animated form export after reopening, partial influence,
extrusion/subdivision, modifier application, join/separate, inset, I/K shortcuts,
modified-key removal, rename/bake, undo and serialization. See `VERIFICATION.md`
for the current accepted package and remaining surface limitations.
