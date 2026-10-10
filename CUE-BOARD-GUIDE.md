# Animation cue boards

In Build, select animation keys and choose **Cue selected frames** from the animation toolbar or context menu. The Animation Cues panel opens with the selected range. Give the cue a name, choose its total play count, and optionally hold its last frame for a number of seconds or until manually continued. Four plays means four total passes, with a hold after each pass. Loop repeats until stopped.

Open the saved element in Studio, then add its authored cues to the vertical cue board. Select a play to preview it privately. Expand a cue to see its repetitions and any child cues. Preview does not take the element on air.

Each repetition can override exposed text fields without changing the source animation or other repetitions. Choose the field and enter its replacement text. **Use original text** restores inheritance. Right-click a repetition and choose **Modify in Build** to create an independent scene for that repetition. Saving returns those edits to that section; new animation cues remain inside it, ordered by their source frames. Child cues can be disabled individually.

Save the board to keep the draft. **Render board** starts it on the broadcast clock; if playout is paused, it is armed until playback resumes. **Continue section** advances past a held or looping section. **Hide** removes it from output. Finite boards can also be placed on the timeline with the clock control. Their duration includes every repetition and timed hold. Manually held and endless boards require live control and cannot be placed as fixed-duration clips.

Build and Studio check for conflicting changes before saving a repetition or board. If another edit changed the same source, reopen it to load the latest version. Source scenes and other repetitions retain their independent content.

**Source content and thumbnail** retains the original scene fields, media replacement and thumbnail tools. **Animation schedules** retains individual cue triggers and program/ad timers. A newer scheduled trigger can replace a playing or held board; manual triggers win ties. Previewing a cue remains private.
