# Cuebase — Windows preview

Cuebase brings scene creation, animation and broadcast planning into one workspace.
This is a development preview, not a production-ready release.

## Highlights

- Form Animation: capture confirmed mesh moves and extrusions, adjust influence,
  and insert or remove influence keyframes. Hover Influence and press I or K to key it.
- Ordered offline audio effects: echo, EQ, high/low-pass filters, compressor,
  limiter and gain. Reorder or bypass effects and render a separate WAV copy.
- Shared audio library, source audition, scene timing, fades and mute controls.
- Object Z-index controls and forward/backward layer actions.

## Install and run

Download the Windows ZIP from the release assets and extract the entire archive
to a writable folder. Run Cuebase.exe from that folder; retain the bundled DLLs
and plugins. Save your work and close the running application before replacing it.
An unsigned preview may trigger Windows reputation prompts; no code-signing
certificate is claimed for this build.

## Known limitations

- DSP processing is offline and creates a new file; it is not a live mixer insert.
- Form influence operates on stable vertex indices. Incompatible topology edits
  disable old forms rather than assigning their offsets to unrelated vertices.
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
