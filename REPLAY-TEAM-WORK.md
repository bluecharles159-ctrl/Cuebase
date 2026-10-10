# Replay, effects and production collaboration — verified update

User requirements (2026-10-09):
- Right-drag timeline range, Replay frames context action and frame-accurate start/end modal.
- Entrance/return effects, program defaults, pause main scene opt-in only; resume correctly.
- Separate replay clock/audio/video; main scene continues by default.
- Effects saved in Screen Elements / Effects; create/edit in Build with no default rectangle.
- Animated full-frame effects and composable presets, including fade, zoom, pulse/breathing, desaturation and slowdown.
- General editable text fields (Sponsor is only an example).
- Crew collaboration with permissions and conflict protection.
- StudioBinder-inspired production workflows, informed by public feature/tutorial material.
- Replace the existing Studio footer with compact operational status; narrow scrollbars and consistent icons.

Decisions: host-controlled studio LAN/VPN sessions; viewer/editor roles. TV priorities are crew contacts, assignments, schedules, audio/visual rundown notes, call sheets and reusable graphic text.

Implemented and verified: replay controller and audio crossfades, range dialog, program effect defaults, Build effect controls and presets, TLS crew portal with per-member invitations/revocation, optimistic conflict checks, production tables/call-sheet export, atomic library saves and compact footer. Live segments without a recording are rejected by replay; this is timeline-media replay, not a live-input DVR.

Build repair: Code::Blocks is not required; MSYS2 UCRT64 remains installed. User reported assembler file-write failure. The machine has 4 GB RAM and simultaneous builds left about 350 MB free. Default preset now uses one compiler job, one autogen job and compact debug information. Build-Cuebase.cmd launches only after successful compilation. Do not claim resource pressure is proven to be the original failure's cause.

Research reviewed: StudioBinder homepage, task board, call sheet and AV script feature pages; Academy overview, collaboration and Write/Breakdown/Visualize/Plan/Shoot course pages. Embedded videos have not been watched; do not claim otherwise.

Implementation order: effects/rendering and Build authoring; replay controller/render/audio and timeline selection; program UI/defaults; production data/UI and authenticated collaboration; footer/polish; integration tests, visual review, documentation and package/commit.

Baseline: beafa94 and 4dddeae. 21 Build and 13 Studio/output cases passed, plus a fresh-process RTMP/SRT recheck. Preserve user data, original ZIP, notes and unrelated generated build changes.

Verification completed 2026-10-10: single-job application/test build; 22 Build and 16 Studio/output cases passed. Includes local RTMP/SRT recording replay and TLS role/revocation/conflict checks. Rounded checkbox states and effect/crew/Studio layouts reviewed through Qt offscreen screenshots. Exact Bezier key values and decoder readiness were corrected after initial failures. Native desktop interaction remains unverified.
