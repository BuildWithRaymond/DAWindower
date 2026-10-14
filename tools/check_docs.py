"""Check source-document links and Markdown anchors without network access."""

import re
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parent.parent
LINK = re.compile(r"!?\[[^\]\n]*\]\((<[^>]+>|[^\s)]+)(?:\s+[^)]*)?\)")


def prose(path):
    """Blank fenced examples so their illustrative links are not treated as links."""
    fence = None
    lines = []
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        marker = re.match(r"\s*(`{3,}|~{3,})", line)
        if marker:
            if fence is None:
                fence = marker[1][0]
            elif marker[1][0] == fence:
                fence = None
            lines.append("")
        else:
            lines.append("" if fence else line)
    return "\n".join(lines)


def anchors(path):
    result = set()
    repetitions = {}
    for line in prose(path).splitlines():
        heading = re.match(r"^#{1,6}\s+(.+?)(?:\s+#+)?$", line)
        if not heading:
            continue
        # GitHub removes punctuation, retains Unicode letters and suffixes duplicates.
        text = heading[1].lower()
        slug = "".join(c for c in text if c.isalnum() or c in " -_").replace(" ", "-")
        count = repetitions.get(slug, 0)
        repetitions[slug] = count + 1
        result.add(f"{slug}-{count}" if count else slug)
    return result


def main():
    files = sorted(
        set(ROOT.glob("*.md"))
        | set((ROOT / "docs").rglob("*.md"))
        | set((ROOT / ".github").rglob("*.md"))
    )
    errors = []
    checked = 0
    for source in files:
        text = prose(source)
        for match in LINK.finditer(text):
            target = match[1].strip("<>")
            url = urlsplit(target)
            if url.scheme or url.netloc:
                continue
            checked += 1
            destination = (source.parent / unquote(url.path)).resolve() if url.path else source
            line = text.count("\n", 0, match.start()) + 1
            prefix = f"{source.relative_to(ROOT)}:{line}: {target}"
            try:
                relative = destination.relative_to(ROOT)
            except ValueError:
                errors.append(f"{prefix}: outside the source repository")
                continue
            if relative.parts and (
                relative.parts[0] in {"dist", "runtime-tests"}
                or relative.parts[0].startswith("build")
            ):
                errors.append(f"{prefix}: generated/local artifact; use a plain code path")
            elif not destination.exists():
                errors.append(f"{prefix}: missing target")
            elif url.fragment and destination.suffix == ".md" and unquote(url.fragment) not in anchors(destination):
                errors.append(f"{prefix}: missing Markdown heading")
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"Documentation links passed ({checked} local links in {len(files)} documents).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
