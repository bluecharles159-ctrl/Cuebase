# Cue workflow revision and broadcast continuity

User request following commit 51dd162. Latest follow-up clarifies editable/getError messages, private live-monitor preview, split layouts and production accounts. The previous reference image was deferred; the current green range reference means a temporary highlighted cue range.

Required work, in order:
1. Build cues start empty. Cue home lists named independent ranges with timing/behavior; Create/plus opens a rounded modal, selected keys prefill it, live green timeline range clears on save/cancel. Cue detail has behavior controls, back navigation, and Play restricts playback to that cue.
2. Studio automatic preview for static/animated/cued elements, title/details beside preview, duration/scrubber and current-cue indicator. Only authored-cue elements expose a board. Empty board has a wide Add animation cue action. Add via a searchable picker, drag reorder, duplicate/remove. Independent modified cues and child boards have navigable detail pages/breadcrumbs.
3. Shared timing evaluator for cue repeats/holds and board repeats/holds; finite duration includes every hold and repetition. Infinite/manual behavior clearly identified. Render places an independent baked board snapshot at playhead + configurable delay (default 3 seconds). Duplicate placements remain independent. Future on-air edits require at least five minutes lead time; played/near-air cues protected. Extending beyond original board length repeats the board.
4. Distinct glass/metal light-sweep appearance for board timeline clips; reusable animated light-sweep effect in Build.
5. Main-track programs can be deleted and trimmed/extended, with consistent playout behavior.
6. Emergency output control: black or selected looping quick content, immediately overrides outgoing picture/audio while rundown can be edited, reversible return to program. Branded standby presets using configurable channel logo/messages.
7. Channel setup/profile from header logo: name, logo, description, import or Build. Semantic scene shorthand for logo/break-screen categories, distinct from general editable text fields. Colorful consistent icons and compact cards.
8. Verify timing, snapshots, edit protection, emergency output and main-track editing; review screenshots; update help/docs; build/package/commit without including user data or unrelated generated build files.

Working decisions: total play count retains existing semantics (3s + 2s hold, two plays = 10s). No audience metrics/CDN assumptions. Continue on existing source; retain saved library compatibility. No subagents requested or used.

Status: built successfully; combined Qt verification passed 26 Build and 21 Studio/output cases. Final error-retention and cue-layout refinements are being rechecked:
- Modal Build cue creation, green temporary ranges/drag selections and selected-cue playback.
- Automatic Studio cue previews, board cards, nested independently editable sections, per-cue and whole-board timing.
- Baked timeline snapshots, configurable render delay, duplication onto a free layer, five-minute edit guard and extension repeat.
- Glass timeline clips and authorable metal light-sweep effect.
- Black/standby output override, configurable looping content, editable messages and explicit sanitized real-error selection; program and replay audio are suppressed during override.
- Header channel profile with imported/saved-element logo, name and description.
- Private preview retains animated live picture-in-picture; Return to live uses preview-only nine-tile transition; scrubber covers one segment.
- Main-track removal and explicit duration edits with downstream placement shifts; frame input drag focus and volume wheel sizing.

Not yet implemented: production accounts/channel isolation/multi-machine native editing and timed multi-source split layouts. Final updated distribution is pending verification. Visual artifacts have been reviewed with the offscreen Qt platform; native desktop interaction is not claimed. These remain active requirements, not release claims.

Design references: Figma dashboard templates (https://www.figma.com/templates/dashboard-designs/) and component hierarchy guidance (https://www.figma.com/best-practices/components-styles-and-shared-libraries/); Pinterest Broadcast UI collection (https://in.pinterest.com/happymimi8/broadcast-ui/). Use consistent compact cards, preview/title hierarchy and semantic icon accents; original implementation, no copied assets.

## Follow-up requirements (same active task)
- Emergency messages: editable copy and selectable real error/status messages for display; never expose secrets/stack traces automatically. Presets still configurable.
- Green cue range applies to drag-selected keys as well as frame-entry dialog.
- Viewport scrubber needs draggable head and current-program duration. Scrubbing privately previews without seeking broadcast. Live picture remains as bottom-right picture-in-picture with subtle transition. Return to live uses short nine-tile reconnect animation/overlay; it affects preview only.
- Viewport split-screen controls with five animated layout presets; timed changes (e.g. 2 feeds for 10s, then 1, then 3), editable sources and smooth transitions.
- Default one channel; first-run production/account setup can be skipped for graphics editing. Production profile owns isolated channels; optional shared settings; open channel in separate window. Multi-machine crew workflows for programs, graphics and audio via authenticated host session. Be explicit about local/host identity versus any public cloud account; no fabricated hosted service.
- Volume sliders respond to vertical wheel while ordinary numeric inputs retain horizontal-drag behavior.
- Fix backdrop blur rendering as white/opaque in Studio despite Build opacity.
- Future release intent: monetization after functionality, GitHub download, promotional assets and free installer with three-month trial/site. Do not publish credentials, launch paid services or claim deployment before implementing/reviewing these later release tasks.

Current candidate also adds main-program drag trim edges and editable ad-spot lengths, scene-name classification via {{element:main-logo|Station logo}} / {{element:break|We will be right back}}, and Save/Discard/Cancel protection for timeline-board drafts. Combined build/test verification passed (47 Qt cases including setup/cleanup). Current single-job build log: build-final/latest-build.log.

## Latest follow-up
Markdown text objects use Qt GitHub Markdown with HTML disabled and a persistent Typography format selector; renderer/persistence regression coverage added. Programs gains searched thumbnail cards and Grid/List selection; cue-board padding and card spacing increased. A replay test fixed its overly short event-loop assumption by waiting for observable clock advancement with a bounded timeout. These refinements are undergoing verification.

Requested Google/GitHub/email sign-in and cross-device account/channel data require a provisioned authentication/data service and OAuth provider registrations. No simulated sign-in buttons or fabricated account success were added. Production onboarding, pricing/access tiers and cloud synchronization remain outstanding; asked whether to prepare Supabase or use an existing service. Streaming setup must separate channel identity from endpoint/secret credentials; a logo/name alone cannot establish network output.

## Production benchmark and additional editor work — 2026-10-11
User prioritized an HGTV renovation-promo comparison, recreated in installed Blender and Cuebase, with saved editable projects and rendered evidence, closing feature gaps observed in that comparison. Also requests at least one hour of audiovisual references across HGTV, Disney Junior, Nickelodeon, M Movie Zone, NatGeo/Discovery/Science, Studio Universal and E. No claim that the hour has been collected or watched yet.

Public HGTV designer source located: https://nathanielhowe.com/projects/2023_hgtv_refresh/prj_index.html . The page exposes short toolkit assets and a final m_20.mp4 reel. Its custom_00.mp4 downloaded as reference-research/hgtv-toolkit-reference.mp4 (2.002 seconds, 600x338, video/audio). Downloading the final reel. These copyrighted reference files stay outside the source distribution and commit. Text case-study research is separate from actual audiovisual inspection.

Computer-use skill initialized twice; both attempts timed out and reset the kernel before selecting any window. No native Blender/Cuebase GUI action or manual comparison claimed. Installed Blender 5.1 command-line path exists; checking background mode as fallback.

New source work: per-object inspector expansion keys (fresh objects Transform only, Text also Typography), circular imported-logo header container, and screen-element Z index stored in cueBoardSettings so baked board order stays independent. Studio renderer collects visible graphics then stably sorts by that order before composition, preserving old order at equal values. Added per-object state and overlay-order regression cases; verification pending.

Still pending: fixed-center moving Studio timeline; audio waveforms aligned with clip offsets; optional thumbnail strips; settings category sidebar with Timeline options; control-panel category rail and real audio processing/meters; reference-style dropdowns; import/export expansion including baked animations and editable assets; comprehensive benchmark renders and gap fixes; exact HGTV match. Earlier pending accounts/multi-channel/native crew and scheduled split layouts remain. Never represent these as finished controls or release readiness.

## Imported channel logo — 2026-10-11

The header channel profile clips imported and saved-element logos into a circular container, with proportional fitting and antialiasing. The application build succeeded. Native desktop visual review remains unverified because the computer-use runtime could not initialize.

## Verified checkpoint — 2026-10-11

Application build succeeded. Final editor suite: 28 passed, 0 failed (100.04 seconds CTest); final Studio/output suite: 22 passed, 0 failed (239.29 seconds CTest), including RTMP/SRT loopback. Per-object inspector, Markdown and Z-index checks pass. The per-object test now flushes Qt deferred deletions before locating the current panel. Earlier counts above describe intermediate checkpoints. Pending production benchmark, timeline/audio and account requirements remain pending.

## Render/export checkpoint — 2026-10-11

Added a real PNG sequence export in Build and the CuebaseRender console tool, with rational frame rates, explicit frame ranges, transparent output, a timing manifest, empty-folder protection and cancellable UI export. Audio mixes, Studio rundown exports and cue-board baking remain separate unfinished requirements. Added object-space paint reveal controls (progress, roughness, softness, direction) and native triangle shape selection. Visual review identified patterned reveal noise; the hash was improved and both suites passed afterward: 29 editor + 22 Studio/output cases, 88.61 seconds combined CTest. Commit 6091aad preserves the preceding verified checkpoint.

Native Cuebase and Blender files for the short HGTV gable ident have been authored under benchmarks/hgtv-toolkit. The reference is 48 frames at 24000/1001 fps. Shared imported texture is the clean opening plaster frame; geometry, text and animation are authored separately. This is not yet the requested full renovation promo, exact visual match or one-hour multi-channel research. User tentatively confirmed M-Net Movies Zone as the ambiguous channel.

## Render comparison delivery — 2026-10-12

Visual comparison exposed the need for an outward reveal; added Linear/Radial mode with keyed center X/Y, and verified hidden/visible/moved-center behavior. Build import now resolves relative image texture paths beside a composition, covered by an import test. Application rebuild succeeded, followed by 29 editor + 22 Studio/output cases (40.22 and 46.43 seconds respectively; 86.71 seconds combined CTest), including localhost RTMP/SRT. No source changes followed these checks except documentation/package paths.

Both native projects rendered all 48 frames at 320×180. Generated and inspected the labeled comparison contact sheet; encoded and probed all 48-frame 24000/1001 fps videos. The short ident reconstruction remains approximate in font, paint edges and timing, with no recreated audio. SSIM at matched low resolution: Blender 0.881512; Cuebase 0.888144, substantially helped by the shared plaster texture. The comparison report explicitly records these limitations. Separate private benchmark archive keeps third-party reference assets outside the application source package.

The four additional images sent on 2026-10-12 are deferred references: user will explain them later. Do not infer new implementation instructions from them yet. Existing timeline/audio, production-account and full renovation-promo requirements remain pending.
