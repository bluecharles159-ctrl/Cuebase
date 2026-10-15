# Planner and physics work — 2026-10-14

The user's direct instructions take precedence over generated suggestions in the
attached brief. The Studio Blue checkpoint is committed as c9a3d96; its 36 Build
and 23 Studio cases passed before the following work began.

## Rigid bodies (implementation in progress)

The old solver used world-aligned bounding boxes, linear velocity and positional
separation. It had no angular velocity/contact torque, so it could leave an
overhanging block supported unrealistically. Bullet 3.25 has been installed using
MSYS2's UCRT64 package. Its installation hook was rerun with MSYS2 tools on PATH.

The replacement uses convex hulls for dynamic objects, triangle collision meshes
for passive meshes, thin prisms for flat surfaces, inertia and contact impulses,
friction, restitution and fixed 8 ms steps. Scene copies detach simulation state;
backward scrubbing rebuilds it. Tests cover stable support, overhang tipping,
deterministic rewind, independent scene copies and parented bodies. Both focused
physics cases now pass, including flat canvas-plane objects, moving passive
supports and gravity/ground edits invalidating cached simulation state.

The parent test compares equivalent world-space scenes rather than assuming a
falling body cannot move sideways on impact. Multiple convex/plane contacts are
enabled to improve flat landing stability. Animated passive transforms are
orthonormalized before reaching Bullet; scale/shear never enters a rigid basis.
The full Build suite passed all 38 cases (72.8 seconds), the application and
renderer rebuilt, and all 23 Studio cases passed. Native interaction remains
outstanding. Exported frames at 0, 2 and 4 seconds were inspected. See
`build-final/physics-results.txt` and `build-final/physics-full-results.txt`.
Bullet's upstream license is in `assets/licenses`.

Remaining checks: rotated/inclined support, thin active mesh surfaces, animated
passive transforms, render/export equivalence, collision geometry/margins, cached
simulation invalidation, complete regression suite and native inspection.
Collision shapes currently use initial scale; deformation/non-rigid scaling is
not a physically simulated feature. Dynamic concave meshes need decomposition
or compound shapes; a convex hull does not preserve interior cavities. Bounds
centers currently define centers of mass; complex solids need a more precise
volume-based option. These limits must remain explicit in the capability audit.

Primary references:
- Autodesk's MassFX distinction between graphical meshes and physical collision
  shapes, dynamic convex shapes and concave decomposition:
  https://help.autodesk.com/cloudhelp/2018/ENU/3DSMax-Tutorial/files/GUID-60A7521E-B5FD-4165-9A0E-EAAD7FBB5D71.htm
- Mass, static/dynamic friction and restitution:
  https://help.autodesk.com/cloudhelp/2020/ENU/3DSMax-Simulation-Effects/files/GUID-62BA863E-A0C4-4CA7-95F1-720F00AE8EA7.htm
- Bullet's implementation and zlib licensing:
  https://github.com/bulletphysics/bullet3
  https://github.com/bulletphysics/bullet3/blob/master/LICENSE.txt

## Planning board — interactive checkpoint

The interactive board is wired to the top-right workflow icon. It has a separate
Library/Playout preview, source filters, a mixed sequence, reorderable list, moving
playhead, rich-text/image cards, four-sided connections, undo/redo and checked
persistence. Unreadable saved plans remain untouched until explicit recovery,
which keeps the original data. Crew-setting edits preserve planner data.

All 27 Studio cases pass, including four planner tests; the independent model's
six cases passed at the previous checkpoint. Offscreen board/canvas screenshots
were inspected. Native interaction, real-media coverage and explicit promotion to broadcast remain
pending. Window geometry is saved, the empty-card plus opens its content menu, and the
note-image cache is bounded to 32 MiB. The app rebuilt and all 6 focused cases
passed after those refinements. Native computer-use initialization failed with
a missing-path error. The overall request and release checklist remain incomplete.
Blender's official node-editor and reroute documentation was consulted for
socket/link behavior. Cuebase's user-requested many-to-one note links intentionally
allow more connections than a Blender reroute input. No reference video has been
watched in this pass:
https://docs.blender.org/manual/en/5.0/interface/controls/nodes/node_editors.html
https://docs.blender.org/manual/en/5.4/interface/controls/nodes/types/layout/reroute.html

- Keep its model, private preview clock and editable sequence separate from live
  playout. Opening, playing or scrubbing a plan must not alter broadcast state.
- A single mixed sequence contains program, ad and screen-element references,
  explicit durations and source metadata. Unknown/manual hold durations require
  a finite duration before promotion to a timed broadcast schedule.
- Selector filters program/ad/screen-element sources; drag/drop or context action
  appends/inserts. Reorderable thumbnail list and timeline reflect one model.
- Private preview above the timeline; moving playhead on the virtual timeline.
  Timeline starts compact and expands with content, with zoom and pan.
- Notes/images occupy a canvas below the timeline. Drag empty space to create a
  card; empty cards show animated dashed outlines. Text edit controls appear in
  the header. Resize, lock, colors, image fill, rich text and gesture undo.
- Four connection ports per card, shared endpoints without duplicate dots,
  many-to-one/one-to-many links, timeline-item attachments and line style menus.
  Connections track card movement and timeline reorder. Cards avoid timeline
  geometry. Do not claim hardware haptics on unsupported devices.
- Nonmodal resizable floating window with a short opening animation, full-screen
  toggle and saved geometry. Closing stops private preview and saves the plan.
- Persist plans in channel-scoped library data with checked/atomic saves; source
  IDs and imported media references must survive reload. Promotion is explicit,
  validated and undoable, never an incidental consequence of planning.
- Verify source-library isolation, reorder, durations, persistence, missing media,
  card geometry, connection endpoints, preview state and undo transactions.
