# Graphics, animation cues and Studio

## Entrance, hold and exit

1. In Build, animate an entrance and an exit on the composition timeline.
2. Open **Animation Cues** in the inspector rail. Add a cue, name it **Entrance**, and enter its start and end frames. Enable **Hold on screen after animation**, then Apply.
3. Add **Exit**, enter its frame range, disable Hold, and Apply.
4. Save the composition. The message is **Saved to screen elements**.
5. In Studio → Screen Elements, select the saved overlay. Trigger Entrance to play it once and hold its exact final frame. Trigger Exit to replace the entrance and hide the overlay when the exit ends. Triggering either again restarts that animation.

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
