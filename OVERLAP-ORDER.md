# Controlling overlapping screen elements

In **Screen Elements**, select an element and set **Z index**. Higher values draw
above lower values. Negative values are supported. Equal values retain their
existing relative drawing order.

To change only one timeline copy, right-click that element's clip and choose
**Overlap order…**. Clear **Use source order**, enter a Z index, and save.
The change stays private until saved and does not change the source element or
other copies. Cancel leaves the copy unchanged.

Check **Use source order** to remove the override. A regular placement follows
its library source; an independently rendered cue-board copy follows its saved
snapshot. Changes to the library source do not rewrite that snapshot.

The chosen order is saved with the timeline. A failed save retains the previous
order. If another edit changes the same copy's order while the dialog is open,
reopen the dialog to review the current value before saving.

This control orders screen-element overlays. It does not change the internal
object hierarchy of a Build composition or route audio.
