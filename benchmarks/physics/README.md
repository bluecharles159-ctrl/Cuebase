# Support and overhang regression scene

Open `support-and-overhang.cuebuild` in Build and play from frame zero. The blue
block is centered over its passive support. The orange block's center is beyond
the right edge of its support; contact should rotate it and it should fall to
the ground. Both shapes use the canvas-plane constraint.

This is an authored physics regression scene, not an HGTV recreation or a claim
of complete production feature parity. Numerical support, tipping, replay and
parent-transform checks are in `BuildTests::rigidBodySupportAndReplay`.

The renderer exported five frames; frames at 0, 2 and 4 seconds were visually
inspected on 2026-10-14. The blue block remained on its support and the orange
block landed on the ground. Native viewport interaction remains pending. The
scene uses only built-in shapes and needs no external media.
