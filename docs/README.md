# Documentation

Start with the project [README](../README.md) and [contributor guide](../CONTRIBUTING.md).

## Working guides

| Guide | Read it when… |
| --- | --- |
| [Development](development.md) | Setting up tools, building, formatting, inspecting packages or enabling diagnostics. |
| [Architecture](architecture.md) | Following a frame, changing hooks or checking lock/reference ownership. |
| [Testing](testing.md) | Choosing a regression, running actual-client checks or recording a manual transition. |
| [Launcher integration](integration.md) | Replacing a dawnd resource/injection path or inspecting the optional export ABI. |
| [DAWindower X](x-edition.md) | Building the edition with extra client controls or understanding host-owned window titles. |
| [Screenshots](images/README.md) | Comparing the two editions and checking original captures, dimensions and scope. |
| [Changelog](../CHANGELOG.md) | Checking what changed across runtime versions. |
| [Credits](../CREDITS.md) | Understanding dawnd's legacy and the research references. |

## Acceptance and historical research

These reports keep their original scope and dates. A statement about a prototype
is not a requirement for today's standalone DLL.

| Record | Scope |
| --- | --- |
| [Downscaling performance](scaling-validation.md) | Reproduced RGB565 shrink cost, 32-bit presentation, exact-pixel regression and packaged standard/X CPU measurements. |
| [Game icons and native caption spacing](icon-validation.md) | Version 0.2.3 icon restoration in both editions and standard native caption spacing. |
| [X and borderless candidate](x-borderless-validation.md) | Version 0.2.3 candidate; both editions, launcher fullscreen and scoped client geometry guards. |
| [Free resize and visible frame cursors](smooth-resize-validation.md) | Versions 0.2.1–0.2.2; current resize/cursor and exact package evidence. |
| [Standalone loading and recovery](standalone-validation.md) | Version 0.2.0; actual injection, multiclient tests and recovery limits. |
| [Recovery design](standalone-recovery-design.md) | Rationale for import gates, retained pixels and per-window recovery. |
| [October 14 recovery assessment](rendering-recovery-assessment.md) | Historical pre-0.2.0 findings and proposed remedies. |
| [Baseline investigation](investigation.md) | Inspected legacy binary/client fingerprints, API paths and research scope. |
| [Prototype validation](validation.md) | Historical 0.1.x results, including explicit-init/proxy development paths. |

Raw logs and binaries mentioned in reports are ignored local artifacts. Public
source links point only to files included in this repository.

## Design records

These capture decisions at a specific point in development. Use the working
guides above for the current integration and runtime contract.

| Record | Status |
| --- | --- |
| [Window title ownership](designs/2026-10-15-window-title-ownership.md) | Current title contract and its verification: both editions display Windows window text. |
| [Shared character title](designs/2026-10-15-shared-character-title.md) | Superseded; describes the removed character-memory reader. |
| [Original X design](designs/2026-10-14-dawindower-x.md) | Historical; standard now shares the caption renderer, and neither edition reads character memory. |
