# Cuebase — Windows preview

Cuebase brings scene creation, animation and broadcast planning into one workspace.
This is a development preview, not a production-ready release.

## Highlights

- Form Animation: capture confirmed mesh transforms, extrusions and insets, rename
  forms and key their influence. Topology operations transfer earlier forms, and
  join/separate preserves their animation. Hover Influence and press I or K.
- An editable Form Animation example with exported start/end frames.
- Origin relocation preserves surface pixels, animation and child placement.
- Separate Final and Work pictures with independent Front/Right/Top work views
  and saved output-format presets.
- Graph curves follow channel selection, highlight the active curve and retain
  clipboard/duplicate undo behavior.
- Ordered offline audio effects: echo, EQ, high/low-pass filters, compressor,
  limiter and gain. Reorder or bypass effects and render a separate WAV copy.
- Shared audio library, source audition, scene timing, fades and mute controls.
- Object Z-index controls and forward/backward layer actions.

## Install and run

Download the Windows artifact linked in README.md and extract the application
ZIP inside it to a writable folder. Run Start Cuebase.cmd; retain the bundled DLLs
and plugins. Save your work and close the running application before replacing it.
An unsigned preview may trigger Windows reputation prompts; no code-signing
certificate is claimed for this build.

## Known limitations

- DSP processing is offline and creates a new file; it is not a live mixer insert.
- Form topology transfer rejects incompatible changes. Text/shape/image surfaces
  keep their original look on conversion; outward effects on deformed faces and
  combined surface materials still need further work.
- The broader feature backlog remains incomplete. Account synchronization still
  requires service configuration. macOS and Linux packages are not included.
- Back up valuable projects before testing this preview.

## Report a problem

Include your Windows version, build commit, steps to reproduce, expected result,
actual result and a small sample project when possible. Remove private media and
account credentials before attaching files to a public issue.

## Publication gate

Replace this section with the exact version, commit, SHA-256, tested Windows
configuration and verified download link before publishing. Attach only artifacts
from the final successful workflow run. Do not substitute an older build under a
new version label. Publisher name remains unassigned pending the owner's choice.
