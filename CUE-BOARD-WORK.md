# Animation cue board — active work

Requested in the attachment `f22ea1be-ffb4-4ac6-b624-2eec67b5b298/Pasted text.txt`, after completing the replay/crew update. Baseline commit: 8888fbf. Baseline source ZIP: Cuebase-replay-crew-update-2026-10-10.zip (SHA256 3feaec6c740603ffa71df3693eb46ae7a71bc871917eac9ea60260e8859e3a24). Baseline verification: 22 Build / 16 Studio cases passed, including real RTMP/SRT recording replay. Rounded checkboxes implemented and reviewed.

Requirements:
- Build cues have name, frame range, replay/repeat count, indefinite loop, timed/manual hold. A count of 4 means four total plays; timed hold follows every play.
- Select timeline/graph keys → Cue frames → automatically open Animation Cues with the selected range, ready to name/configure.
- Studio element details replace the library view with a named subheader/back navigation, private preview, and a vertical cue board.
- Add authored cues to the board, display range/duration/repeat/hold using compact icons and labels. Reorder whole entries while keeping child cues attached to their parent.
- Preview a selected board entry privately. Render board explicitly takes the sequence to air. A neighboring scheduling control places the board at a validated timeline time.
- Each repetition can independently override any exposed text field, with field labels and inherited defaults. Changes must not mutate source animation or other repetitions.
- Right-click a repetition → Modify in Build creates an independent design snapshot. Saving updates only that repetition. Any cues authored in that snapshot become children, ordered by source frames; children can be disabled and cannot be moved outside the parent.
- Preserve existing saved scenes and legacy entrance/exit schedules; carry frame/hold timing through video, audio, paused rendering, replay and timeline placements.

Implementation direction:
- Pure cue sequence compiler/evaluator shared by preview and playout; compile finite repetition steps, exact frame boundaries, manual holds and indefinite loop groups, with explicit Next section/Hide controls.
- Store the Studio board separately on Element as cueBoard JSON; each entry references a source cue and owns sections (per-repetition field overrides and optional independent design JSON).
- Cache compiled plans per element source/board signature. Playout samples a plan and passes the resolved design to both graphics and audio renderers, without mutating Library.
- Build variant context stores element/board-entry/repetition identity and checks for conflicting saves. Child cues live inside the variant design, not the source scene.
- Test exact duration/hold/repeat boundaries, text and keyframe isolation, children/disabled cues, preview/live parity, persistence and UI integration. Build with one job on this 4 GB machine; do not touch Cuebase_test or unrelated generated/user files.

Status: implemented and verified. The final single-job build succeeded. All 24 Build and 17 Studio/output test cases passed (58.84 seconds total in CTest), including actual localhost RTMP/SRT recording replay. Reviewed Qt offscreen screenshots of the cue board and embedded Studio details; tightened spacing, minimum control heights and scrolling. Legacy timers, source content/media controls and thumbnail editing remain accessible. Later scheduled cues override boards; manual triggers win ties. Source/repetition saves check conflicts and persistence failures. Help and CUE-BOARD-GUIDE.md document the workflow. Source package: Cuebase-cue-board-update-2026-10-10.zip. The latest reference image is intentionally deferred until the user explains it.
