# Display color in Build and output

The project's display view, contrast look, exposure and gamma apply to Build's
Final view, the clean composition preview, Studio output and PNG still/sequence
export. They are applied once, after the scene and its motion blur are rendered.
Changing display settings requests a repaint of both Build preview widgets.

Work and Material views retain their editing appearance. The display transform
does not rewrite object colors, materials or animation keys. PNG grading preserves
alpha, including fully transparent pixels. Exported PNGs contain the transformed
colors; applying the same transform again downstream would grade them twice.

Standard / None / exposure 0 / gamma 1 bypasses grading. The existing Standard,
Filmic and Raw transforms use Cuebase's 8-bit display pipeline; this is not an
OCIO-managed or HDR workflow. Transparent graphic exports are graded on their own;
compositing them over another picture is different from grading that entire
composite in Studio.

Broader lighting parity, region cropping and native UI review remain separate
acceptance work. See VERIFICATION.md for the current test status.
