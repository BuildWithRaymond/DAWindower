## Problem and resulting behavior

Explain the concrete trigger, the old behavior and the result of this change.

## Validation

List relevant configurations/tests and results. For runtime changes, separate
synthetic coverage, actual-client loading, manual observations and untested cases.
Include fingerprints when comparing runtime builds.

## Contributor checks

- [ ] Code/documentation checks relevant to this change pass.
- [ ] Runtime changes preserve one x86 DLL, LoadLibrary-only initialization and Windows system dependencies.
- [ ] Public contract changes are documented; credits/licenses are preserved.
- [ ] No game assets or legacy DLL binaries are included.

Mark runtime checks not applicable for a docs-only change. Use the contributor
guide for test selection; a hosted CI artifact is not retail acceptance.
