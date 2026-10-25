# Textures and reusable surface materials

The Build Material tab now contains a texture and surface-preset library. This
implements the asset-library part of the first long brief, section 2. It is separate
from the ordered material/mask stacks still tracked in tasks 53–56.

Import images or portable `.cuemat` presets. Imported images are copied into the
current profile's texture directory; the source is kept. Each library entry has a
stable UUID, name, collection and favorite flag, persisted with the production data.
Search matches names and collections. Filters select collection, textures/presets
and favorites. Double-click or Apply affects selected unlocked text, shape and image
objects. Drag a thumbnail onto a compatible object to apply it to that object alone.
These changes participate in scene undo/redo. Mesh UV materials are not implemented
by this surface-texture path.

Inspector → Surface Texture controls enablement, Fit/Tile mapping, scale X/Y,
pixel offsets, rotation, opacity, tint and Normal/Multiply/Screen/Overlay blending.
Numeric controls can be keyed. The texture is composited over the existing fill;
on text it fills the glyphs. Work mode stays plain. Material, Final, Studio and frame
export use the shared surface renderer. Texture decoding is bounded to 2048 pixels
per axis for rendering and 160 for library thumbnails. Rendering threads reuse a
32 MiB source-image cache; file size and modification time invalidate stale entries.

Save surface preset captures fill, texture settings and material properties. It
excludes text/image content, transforms, geometry, animation, cues and effects. Applying a
preset preserves those destination properties, including animation tracks. A
portable export writes a `.cuemat` composition and a sibling assets folder with
relative paths. Keep both together when sharing. Import copies that preset's texture
into the receiving profile. Full compositions also include texture dependencies
through the existing portable composition exporter.

Relink repairs a library entry and matching references in the current open scene;
other compositions can use Composition media to repair their references. Removing a
library entry keeps managed files because saved scenes may still use them. Failed
metadata writes restore the prior library state and discard only files created by
the failed operation. Original source images and existing scene media are retained.

Verification is recorded in VERIFICATION.md after builds and workflow checks finish.
This library does not implement cross-device synchronization, layered PBR materials,
surface painting, or a texture streaming cache.
