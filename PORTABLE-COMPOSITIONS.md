# Portable Build compositions

In Build, choose Export and select **Portable composition with media**. Cuebase
writes the `.cuebuild` document and a uniquely named sibling `.assets-…` directory.
Keep both together when copying the composition to another folder or machine.
Import the `.cuebuild` file normally; relative image/video and audio paths resolve
against its new directory.

The composition retains the scene's serialized objects, geometry, animation
tracks, animation cues, materials and hierarchy. This is a composition package,
not a production/channel backup or a Studio cue-board package. Those formats and
baked cue-board exports remain separate development work.

Referenced files are copied in chunks; repeated references share one copy. Missing
media identifies the affected object and path before exporting or replacing the
open composition. Empty optional media references are allowed. Cancelling or a
copy failure preserves an existing destination composition. Successful replacement
uses a new media directory so existing packages are not damaged; old sibling media
directories are retained rather than deleted automatically.

In Build's Add → Media menu, **Composition media** lists sources and the objects
using them. Search by object or filename, filter missing files, and select
**Relink selected media**. Replacements stay private until Apply; one Undo restores
the previous paths. A composition changed elsewhere must be reopened in this
window before the draft can be applied.

No audio/video transcoding is performed. A receiving machine still needs a decoder
for the source format. Fonts are not bundled. Third-party font/media rights remain
with their owners.

The package collects object image/video paths and object audio paths. It does not
recursively collect external resources referenced inside SVG or other media files.
