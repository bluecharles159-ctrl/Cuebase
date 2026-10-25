# Text formatting in Cuebase

Select a text object in Build. In **Typography**, change **Formatting** to
**Markdown**, then enter the source text in the content field. The canvas and
export render the formatting while the field keeps the editable source.

| Source | Result |
|---|---|
| `**Tonight**` | Bold text |
| `*New episode*` | Italic text |
| `~~Previous title~~` | Strikethrough |
| `# Programme title` | Heading |
| `- First item` on its own line | Bulleted list |
| `> A quotation` | Quotation block |
| `` `Technical term` `` | Inline code |

Typography controls still set the font, alignment, spacing and color. Plain text
mode displays the symbols literally. Embedded HTML is disabled.

## Reuse the animation with different wording

Enable **Editable in Studio** under **Studio Field**, and give the field a name.
Alternatively, start the content with `{{studio:Headline}}`, followed by the
text. Cuebase removes that prefix from the picture and registers the field as
**Headline**.

In Studio's Screen Elements editor, select the element, change the exposed
content, and choose **Apply content**. The content draft does not change the
source until applied. Updating wording retains the object's animation keys and
Markdown mode.

For a fixed-size graphic, use **Fit long text inside its box**. For a background
that grows with the wording, use text auto-fit in Layout and choose the X and Y
axes independently. Check the resulting composition at the intended output size.

## Media and exports

Use dedicated image objects for portable image assets; embedded Markdown image
resources are not collected by the portable composition exporter. Use composition
media relinking to repair missing linked image/video/audio files.

PNG frame export uses the same text renderer as the composition. Formatting,
editable field registration and animation tracks are retained when the design is
saved and reopened.
