"""Vendor the small Lucide subset used by Cuebase, with provenance and license."""
import json
from pathlib import Path
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "assets" / "icons" / "lucide"
NAMES = "sliders-horizontal align-horizontal-justify-start chart-spline list-video boxes globe sparkles square-check pipette chevron-down chevron-right arrow-left arrow-right undo-2 redo-2 save plus eye eye-off lock-keyhole lock-keyhole-open maximize minimize search image film music audio-lines radio monitor-play layers box circle square triangle type move rotate-3d scaling play pause skip-back skip-forward repeat key-round diamond trash-2 rotate-ccw pencil ruler crosshair settings panels-top-left camera sun moon paintbrush folder-open download upload scissors volume-2 circle-help info x check list grid-2x2".split()

def fetch(url):
    with urlopen(Request(url, headers={"User-Agent": "Cuebase-icon-vendor"}), timeout=30) as response:
        return response.read()

commit = json.loads(fetch("https://api.github.com/repos/lucide-icons/lucide/commits/main"))["sha"]
base = f"https://raw.githubusercontent.com/lucide-icons/lucide/{commit}/"
DEST.mkdir(parents=True, exist_ok=True)
for name in NAMES:
    source_name = {"trash-2": "trash", "circle-help": "circle-question-mark"}.get(name, name)
    data = fetch(base + f"icons/{source_name}.svg")
    if b"<svg" not in data:
        raise ValueError(f"Invalid SVG: {name}")
    (DEST / f"{name}.svg").write_bytes(data)
(DEST / "LICENSE").write_bytes(fetch(base + "LICENSE"))
(DEST / "PROVENANCE.json").write_text(json.dumps({"project": "https://github.com/lucide-icons/lucide", "commit": commit, "icons": NAMES}, indent=2) + "\n", encoding="utf-8")
files = "\n".join(f'    <file alias="{name}.svg">icons/lucide/{name}.svg</file>' for name in NAMES)
(ROOT / "assets" / "icons.qrc").write_text(f'<RCC>\n  <qresource prefix="/icons">\n{files}\n  </qresource>\n</RCC>\n', encoding="utf-8")
print(f"Vendored {len(NAMES)} Lucide icons at {commit}")
