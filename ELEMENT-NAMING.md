# Naming reusable screen elements

In Build, enter a category tag in the element-name field before saving to Screen
Elements. The saved name is the text after the vertical bar. The category chooses
the library collection; it does not replace your composition or its animation.

| Name entered in Build | Saved name | Collection |
|---|---|---|
| `{{element:logo|Corner logo}}` | Corner logo | Logos |
| `{{element:main-logo|Channel identity}}` | Channel identity | Logos |
| `{{element:break|Next program}}` | Next program | Break screens |
| `{{element:ident|Evening ident}}` | Evening ident | Break screens |
| `{{element:lower-third|Presenter}}` | Presenter | Overlays |
| `{{element:title|Tonight}}` | Tonight | Titles |
| `{{element:credits|Production credits}}` | Production credits | Titles |
| `{{element:ticker|Headlines}}` | Headlines | Overlays |
| `{{element:clock|Studio clock}}` | Studio clock | Overlays |
| `{{element:replay|Replay wipe}}` | Replay wipe | Effects |

`main-logo` also selects the saved composition as the channel logo. You can change
that selection later by clicking the channel logo in the title bar. `overlay` and
`effect` are also supported categories.

These tags classify a composition you have built. For example, a `clock` tag does
not automatically create a working clock or a `replay` tag a replay system.
Text content fields use the separate `{{studio:Field name}}` shorthand described
in TEXT-FORMATTING.md.
