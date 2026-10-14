# Graphics, animation cues and Studio

## Build interaction and independent cue plays

**View / Align → Object origins** toggles the small object-origin dots. These are editor overlays and do not appear in exports or program output. **All overlays** also hides them.

Each modifier card has an **Apply** icon. Applying a modifier bakes its evaluated geometry at the current frame. Applying a later stack entry also bakes the preceding entries, because their order affects the result; later entries remain editable. Undo restores the original mesh and stack. In the viewport, **Ctrl+Shift+M** applies the first modifier. Applying is available for unlocked meshes in Object Mode.

The **Mirror object** picker lets you click a visible reference in the viewport without changing the selected object. Escape or right-click cancels. The dropdown remains available for references that are not currently visible. An object cannot be its own mirror reference.

Numeric inputs commit on Enter and release focus. Click again to edit a value. Cue repeat counts use the same compact horizontal-scrub fields as the inspector instead of small stacked native arrow buttons.

**Cue selected frames** opens a rounded modal. Enable **Loop** to edit the **Number of plays**. Choose **Play 1**, **Play 2**, and so on under Playback settings; each has its own optional timed hold or manual Continue. A two-second animation played twice with holds of 0.5 seconds and 2 seconds lasts 6.5 seconds. The frame range belongs to the cue; hold settings belong to individual plays. Saving adds no infinite loop. Existing legacy infinite loops continue playing as before until their behavior is edited.

In Studio's cue board, **Customize play** selects which play's hold and reusable content you are editing. Different board copies retain independent settings. A manual hold has no fixed duration until an operator continues it.

## Reusable text and media

In Build, select a text object and enable **Studio Field → Editable in Studio**. Give the field a useful name such as Sponsor. You can also type `{{studio:Sponsor}}Sponsored by NVIDIA` into Typography. The prefix is removed from the visible text and creates the Sponsor field. Ordinary text stays fixed in Studio.

After saving, select the element in Studio → Screen Elements. Change Sponsor to “Sponsored by Microsoft,” inspect the private preview, then choose **Apply content**. The saved object's animation, position and styling remain intact. Apply also updates any current on-air use of that element. **Reset draft** discards unapplied content edits. Multiple fields are identified independently, including when their labels match.

**Fit long text inside its box** wraps and reduces the rendered font size when needed. Enlarge the text box in Build if the result becomes too small. Image/video objects can expose a Studio field too; replacing their source preserves their animation and fit settings.

For elements with named animation cues, use the new cue board for per-repetition text, private previews and live playback. See [Animation cue boards](CUE-BOARD-GUIDE.md). The original content controls remain available for elements without cues and under Source content and thumbnail for cue-based elements. Existing scheduled cues continue to run and can be edited under Animation schedules.

## Build library, previews and presets

The editable name dropdown in Build opens existing elements. Switching saves changes to the current scene. **New scene**, at the bottom, saves the current scene and opens a fresh one. Cancelling a first-save preview leaves the current scene open.

The first save opens a thumbnail dialog. Scrub to a recognizable frame, keep Full screen or drag a crop rectangle, and optionally add a description and choose a playlist. **Preview** in Build and **Thumbnail** in Studio reopen those settings. Cropping affects the library image only, never broadcast output.

Studio presents thumbnail cards with search and playlists. **Playlist** creates a group; change an element's group through its thumbnail dialog. Starter presets in Build include Sponsor spotlight, News lower third, Be right back and Channel ident. Each has editable text, an animated entrance, a held pose and an exit.

Add entrance and exit cues to the Studio cue board. **Render board** begins playback; **Continue section** advances from a manual hold to the exit. **Hide** removes the graphic. **Animation schedules** retains cue timers and direct triggers.

The placement time follows the playhead until manually adjusted. Arrows step a frame; horizontal dragging adjusts the seconds field. Turn Follow playhead back on to resume tracking. While on air, placement behind the playhead is rejected. Off air, earlier placement is allowed. Cuebase chooses a free overlay lane and reports when all three lanes overlap at the chosen time.

## Studio timeline navigation

**Fixed-center playhead** in the timeline toolbar keeps the broadcast playhead in the middle while clips and the ruler move underneath it, including at the beginning and end of the rundown. The mode is remembered between sessions. Turn it off for the conventional scrolling timeline.

Middle-drag, Shift-wheel or the horizontal scrollbar lets you inspect another part of the timeline without seeking the broadcast. In fixed-center mode this pauses following. Press **F** or click **Reveal playhead** to resume. **Home / Fit timeline** shows the whole rundown and also pauses following. The plain wheel zooms, and zoom preserves the centered playhead while following. Alt-wheel retains the existing explicit playhead-stepping behavior.

## Local streaming tests and monitoring

Open Studio → Monitor and click **Start local test receiver**. Start the rundown, then configure Network Output with one of:

- RTMP: `rtmp://127.0.0.1:1935/cuebase`
- SRT: `srt://127.0.0.1:8890?streamid=publish:cuebase`

Leave the separate stream-key field empty for these complete URLs. Watch locally at `http://127.0.0.1:8888/cuebase`. The receiver binds to this computer only and runs until stopped or Cuebase exits. A public channel requires a reachable streaming server/CDN; this local receiver does not distribute to cable or satellite systems.

Connect Monitor to `http://127.0.0.1:9998/metrics`, path `cuebase`, for receiver counts. The dashboard sums direct protocol readers and HLS sessions, excluding the HLS muxer to avoid double counting. These are connections, not unique people. Today, This week, Last month and Custom range show recorded UTC history; data is collected while Cuebase runs and retained for 63 days. Failed requests show unavailable data instead of zero viewers. No audience history is fabricated before collection starts. History is separated by metrics URL and stream path; switching either shows only that source.

The server integration follows the official [MediaMTX metrics](https://mediamtx.org/docs/features/metrics), [RTMP publishing](https://mediamtx.org/docs/publish/rtmp-clients), and [SRT publishing](https://mediamtx.org/docs/publish/srt-clients) documentation.

## Entrance, hold and exit

1. In Build, animate an entrance and an exit on the composition timeline.
2. Open **Animation Cues** in the inspector rail. Add a cue, name it **Entrance**, and enter its start and end frames. Enable **Keep visible after this animation**, then Apply.
3. Add **Exit**, enter its frame range, disable Keep visible, and Apply.
4. Save the composition. The message is **Saved to screen elements**.
5. In Studio → Screen Elements, select the saved overlay. Add Entrance and Exit to the board. Render the board, then Continue section when ready to leave the held entrance pose. The exit hides the overlay when it ends. Rendering again restarts the board.

Each cue has a timer. **Program time** is an absolute number of seconds from the start of the rundown. **Before ad** and **After ad** are offsets relative to each ad in the rundown. For the requested example, set Exit to Before ad, 10 seconds, then Set timer. A separate Entrance cue may be scheduled After ad or at a later program time. Manual disables that cue's timer.

The most recent trigger takes precedence. Manual triggers win ties. Hide immediately removes the controlled overlay; a later scheduled trigger can bring it back. Pausing the program clock pauses animations and timers. Stopping clears manual trigger state. Seeking recalculates scheduled state at the destination time.

An element placed on a timeline lane starts its first named cue when the clip begins, including its source offset. A held cue remains until the clip ends. Manual and scheduled triggers take precedence over placement playback. Elements without named cues retain their normal composition playback behavior.

## Media and audio

**Shift+A → Media** adds images/SVGs, videos, or audio. Circle and Plane are under 2D Shapes. New objects are white.

The World Background section has an audio icon for adding multiple background tracks. Click a track in that list to open its inspector. Every audio track has independent volume, mute, start time, source offset, playback speed, play length, looping, fade in and fade out. A play length of zero uses the available source duration. These settings affect both preview and the mixed broadcast output. Speed changes can also change pitch; this is not a pitch-preserving audio workstation.

Object Background contains Solid, Gradient and Media controls. Backdrop blur and brightness are in Effects; they process content behind the object inside its silhouette while leaving its own border sharp. Text can have its own rounded background. Corner units can be pixels or percent of the shorter dimension.

## Graph and timeline navigation

- Middle-drag pans. Wheel zooms time. Ctrl-drag zooms along the dominant horizontal or vertical direction. Alt-wheel steps the playhead.
- Grid, ruler, curves and keys share the same coordinates. Selection and value updates preserve the view. Home explicitly fits the view; F frames selected keys.
- G moves keys; S scales keys and handles around their selection center. X or Y constrains either operation. Right-click or Escape cancels an active transform.
- Ctrl+A selects keys on selected graph channels; Ctrl+C/V copies and pastes at the playhead; Shift+D duplicates; I inserts on selected channels.
- Select multiple channels with Ctrl/Shift in the channel tree. X curves are red, Y green, Z blue. Normalize uses a stable value range during editing.
- Interpolation reports the selected keys, or the selected channels if no keys are selected. Mixed means the selection contains different interpolation types. Choosing Bezier updates that scope. Both end handles are available on selected Bezier keys; handle types include Auto, Auto Clamped, Aligned, Vector and Free.
- Summary keys select the underlying property keys at the same frame. The eye menu switches between all objects and selected objects.

Studio clip widths use a consistent time scale. Drag either edge of an overlay clip to trim its duration; left trimming advances its source offset. Wheel zooms Studio's timeline, Shift-wheel or middle-drag pans it.

## Editing

Horizontal numeric scrubbing is one undo transaction. Right-click cancels the active scrub. Screen-edge wrapping supports continued horizontal adjustment. Click to type; a second click places the caret. Enter commits and removes focus. Shift+Enter inserts a line break in multiline fields.

Drag a child onto its parent in Layers, or select children first and the intended parent last, then Ctrl+P. Overflow clips children. Edit Parent Only preserves children's world transforms while the parent is adjusted. Layer search and reveal-selected help locate nested objects.

Studio's Screen Elements library has search and Overlay/Break filters. Select an element to see its animation cues; expand Element details for the basic properties.

## Output

The local output engine sends H.264 video and mixed AAC audio to a configured RTMP/RTMPS or SRT receiver. The producer supplies the destination. Test the receiving station before going live; local loopback tests do not establish delivery to an external broadcaster.
# Replay, effects and crew production

In Studio, right-drag across the timeline to mark a range, then choose **Replay selected frames**. The dialog shows start/end frame numbers (end is exclusive), entry and return effects, and **Pause current scene during replay**, unchecked by default. Confirming takes the replay to air. The Programs panel has **Return from replay** for an early return. Starting another program or stopping playout cancels replay. If the main rundown ends during replay, its last frame holds until replay returns, then playout ends.

Replay uses a separate decoder and clock. The main timeline advances unless you explicitly pause it. Program audio and replay audio crossfade with the transition; timeline sounds and graphic media/audio follow the replay position. Replay works with available timeline media and generated program content. A live input without a recording cannot provide past footage and is rejected. This feature does not record incoming cameras or network feeds into a DVR buffer.

Programs can save default **Replay entry effect** and **Replay return effect** selections. Choose **Create effect in Build** to author an effect without a default rectangle. The Effects tab offers keyframeable dissolve, transform, color, blur, vignette, chromatic offset, grain, scanlines, pulse, shake and playback-speed controls. Fade, zoom reveal, screen breathing, pulse, desaturation/slowdown, flash, soft focus and chromatic impact are starting presets. The same graph editor edits these control tracks. Optional objects can be added with Shift+A. Saved effects appear in the **Effects** playlist and in replay effect selectors. The preview source image is for authoring only; replay applies the effect to the real program picture. These are CPU-rendered effects; output health remains the guide to whether a composition sustains your chosen resolution and frame rate.

**Production / Crew** contains crew contacts and call times, task assignments, schedules, an audio/visual rundown, and a call sheet. Save production applies the draft. Export call sheet creates an HTML document with your production information and tables; open it in a browser to print or save as PDF.

For collaboration, open **Team session** and start the host session. Give each crew member the displayed HTTPS address and a separate Viewer or Editor invitation code. Crew connect from a browser on the studio network or VPN. The session uses a local TLS certificate: compare its SHA-256 fingerprint with the host display before accepting it and entering a code. The host retains playout control. Editors can update production plans and exposed graphic text, preserving animation and styling. Viewers can read the production. Revoking an invitation takes effect on the next request; stopping the session revokes all invitations. Codes remain in memory only and are not saved into the library. The app does not configure your firewall/router or expose an internet service automatically.

If another editor saves first, Cuebase rejects the stale draft and asks you to reload. No changes from a stale draft are silently merged or overwritten. Recent saves record the invitation name and UTC time. Production information is saved with the library through an atomic file replacement.

Editable graphic text is general-purpose: `{{studio:Headline}}Evening news`, for example, creates a field named Headline while displaying only “Evening news.” This works for names, captions, headlines, sponsor names and other text. The prefix is metadata, not visible text. Crew access currently exposes text fields; media files remain under host control.

The planning workflow was informed by StudioBinder's public [collaboration tutorials](https://www.studiobinder.com/tutorials/how-to-collaborate/) and its call-sheet, task and AV-script material. Embedded tutorial videos were not watched.
