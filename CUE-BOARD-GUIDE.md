# Animation cues, preview and broadcast continuity

In Build, open **Animation Cues** and choose **Create animation cue**. You can also select keys and choose **Cue selected frames**. The modal collects the name, start/end frames, total plays, looping and holds. A green range marks the frames while you edit; Save or Cancel clears it. Selecting a saved cue opens its details. **Play this cue** plays only that range; the back arrow returns to the full animation.

Save the screen element and open it in Studio. Its preview starts automatically and never changes broadcast playback. Elements with authored cues have a board with an **Add animation cue** picker. Drag cards to reorder them; right-click to duplicate, disable, remove or modify their keyframes independently. Choose a repetition to edit its exposed text/media fields. **Open child cues** navigates into an independently modified cue's children; the back arrow returns to its parent.

Cue and board duration includes every timed hold and repetition. For example, a three-second cue with a two-second hold and two total plays lasts ten seconds. Manual holds and endless loops show an infinite duration. The preview's next button advances past a hold or loop privately. Whole-board hold/repeat controls apply after the last cue.

**Render** saves the board and places an independent snapshot on a free overlay layer at the current playhead plus the delay, initially three seconds. Set zero for immediate placement or enter a longer delay in seconds. Infinite boards are bounded by the placed clip's end. **Continue on air** advances a rendered copy past a hold/loop; **Hide on air** hides that copy. Neither changes other copies or the source. Extending a finite board beyond its original total length repeats it.

To try a different cut, right-click the placed board and choose **Duplicate on another layer**. Drag the right edge of the copy to repeat the board, then drag the left edge to choose where playback starts. The retained section keeps its timing even when it starts in a later repetition. Open **Edit this cue board** on the copy to change its text or per-play settings.

Right-click a board clip on the Studio timeline to edit that copy or duplicate it onto a free layer. Keyframe/board edits while broadcasting require at least five minutes before the affected section. Played or near-air sections are protected. Closing an unsaved timeline-board editor offers Save, Discard or Cancel.

Hover over the main viewport to use its draggable preview scrubber. Scrubbing opens a private preview while a small bottom-right monitor keeps showing live output. **Return to live** synchronizes the operator view with a short graphic transition; the outgoing stream is unaffected.

**Black / standby** immediately replaces outgoing picture and audio. Expand Standby settings to choose black, a branded message, a saved element, or a looping ad/video. Messages are editable. The error selector copies actual media/encoder errors only when chosen; network destinations are removed from that copy. Return to program restores the existing paused/playing rundown. Click the header channel icon to set the channel name, description and imported or Build-made logo.

In Build's scene name field, enter `{{element:main-logo|Station logo}}` and save to use the scene as the channel logo. Enter `{{element:break|We'll be right back}}` to put a scene in the Break screens playlist. The part after the vertical bar becomes the name in Studio. Changing a main logo to a different category releases its channel-logo assignment. A plain rename keeps its category.

Other scene categories include logo, overlay, lower-third, title, ticker, clock, ident, replay, effect and credits. Inside a text object, use `{{studio:Field label}}Visible text` to expose an editable Studio field.

Production accounts, isolated multiple channels and timed multi-source split layouts remain under development; this update does not provide a public cloud account service.

Text formatting: in Build > Typography > Formatting, choose Markdown for `**bold**`, `*italic*`, `~~strike~~`, headings, lists, quotes and inline code. Plain text remains the default for existing objects. Studio-editable text keeps this format and its original animation. Raw HTML is disabled.

Programs supports search and List/Grid thumbnail cards. Cards use your poster when provided; the fallback is a program icon, not a fabricated thumbnail.
