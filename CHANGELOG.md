# Changelog

Runtime versions below describe the tested behavior. Documentation and tooling
changes do not imply a newly tested retail release.

## Unreleased

- Share X's caption renderer with the standard edition: charcoal-to-black
  shading, game icon, three window buttons and high-contrast support. Standard
  preserves the host's title; character names and extra client actions remain
  X-only. Both editions own the full caption with native caption painting disabled.
- Restore the game's first executable icon in both editions, matching dawnd's
  visible icon behavior. Preserve launcher/class icons; the shared caption paints
  the icon and exposes the native system menu there. No icon asset is distributed.
- Version 0.2.3 candidate: nearest-pixel native 4:3 sizing in both editions,
  caption-only windows in both editions.
- Keep consistent icon/title padding in the shared caption without native side
  or bottom frame margins; include the caption in continuous aspect sizing.
- Continuous corner projection removes reproduced backward size jumps while
  retaining nearest-pixel sizing across native tracking limits.
- X: Dark Ages/character title, charcoal-to-black gradient, button tooltips,
  exact native-size reset and direct suspended/injected New client action.
- Preserve completed launcher popup/fullscreen selection before or after attach;
  suspend windowed constraints there. Guard scoped client geometry/style imports
  while retaining native launcher calls, user data and minimize/hide behavior.
- Candidate acceptance is tracked separately from the published 0.2.2 preview.

- Clarify community-facing comparisons: cursor handling, full-window graphics
  and native resize arrows are existing dawnd behavior. Recovery and open-source
  maintenance are the adoption reasons emphasized by this project.
- Public GitHub repository and a v0.2.2 preview with the exact tested single-DLL
  asset; initial hosted source/Debug/Release checks passed.
- Community-focused README with dawnd credit and an evidence-backed comparison.
- Contributor onboarding, architecture/code tour, development/testing guides,
  documentation index, and issue/PR templates.
- Consistent C++ formatting, EditorConfig, CMake presets and development checks.
- Windows CI definition for builds, unit/source checks and one-file packaging;
  hosted CI does not replace interactive desktop or actual-client acceptance.

## 0.2.2 — 2026-10-14

- Visible native resize arrows on all edges/corners and during resize drag.
- Balanced frame-only cursor visibility; the host retains the game cursor.
- Debug and Release each passed 13 contracts. Actual LoadLibrary-only client
  loading and user-confirmed arrow/game-cursor behavior were recorded.

## 0.2.1 — 2026-10-14

- Independent, smooth width/height resizing without the 4:3 size grid.
- Full-client nearest-neighbor scaling without black bars, with matching mouse mapping.
- Actual one-pixel retail resize sequences and manual visual/input confirmation.

## 0.2.0 — 2026-10-14

- Standalone early LoadLibrary-only initialization through small import gates.
- Retained surface content, stable wrapper identity and bounded loss recovery.
- Per-window presentation retries and session/display/secure-desktop recovery hints.
- One-file distribution with no proxy, initializer call or external configuration.
- Actual isolated-client loading, simultaneous clients and fault-injection coverage.

## 0.1.x — 2026-10-13 to 2026-10-14

- Prototype RGB565 offscreen/GDI renderer and logical software-cursor composition.
- Windowed coordinate corrections and native surface-DC compatibility work.
- Earlier explicit-initialization/proxy comparison routes are historical development
  artifacts; they are not the current distribution contract.

The [documentation index](docs/README.md) links detailed acceptance records.
