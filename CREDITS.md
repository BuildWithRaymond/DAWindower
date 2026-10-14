# Credits

## dawnd and the Dark Ages community

dawnd earned its place in this community through more than twenty years of use.
Its creators, the people who shared it, and the launcher developers who kept it
available made windowed Dark Ages part of everyday play. DAWindower preserves
that simple expectation: one small DLL, injected by the launcher you already use.

The legacy DLL is credited as an important predecessor. DAWindower's implementation
is original C++ code; no dawnd binary or source is packaged. Its MIT license does
not relicense dawnd or the game.

The artifact examined during development was a bundled `DAWnd_v3` binary with
SHA256 `6772726967951c472f7bae53aeb7d70ba2e38605af5f814f9b130c6f04b8e0d9`.
Its PE timestamp is from 2007; that metadata describes this particular artifact,
not dawnd's first release. An exact original release date and author attribution
could not be established from the examined files. Historical primary sources
and corrected credits are welcome.

## Research references

These projects informed investigation and API comparisons. They are not runtime
dependencies, and their source was not copied into DAWindower.

| Reference | Contribution to the investigation |
| --- | --- |
| [DDrawCompat](https://github.com/narzoul/DDrawCompat) | DirectDraw/cursor presentation discussions and recovery design references; 0BSD. |
| [Echo](https://github.com/Sichii/Echo/tree/36d0eeb0709b2e1bd2ba6fa494294fc5b88b72ff) | A historical bundled dawnd artifact and the launcher injection sequence. |
| [Talos](https://github.com/adambraly/Talos/tree/3992da2d22c3bff3ae09bff06d52df3ecc76f177) | Another historical artifact and LoadLibrary-only launcher integration. |
| [Chaos.Client](https://github.com/Sichii/Chaos.Client/tree/05c7050f7144d1e7010f9ee761935783ad5ff2d8) | A reference for logical game dimensions and mouse scaling; AGPL-3.0, no code reused. |
| [Microsoft Windows documentation](https://learn.microsoft.com/en-us/windows/win32/) | DLL loading, DirectDraw resource contracts, GDI presentation and window/input APIs. |

Exact findings and reference revisions are preserved in
[the baseline investigation](docs/investigation.md) and
[the rendering recovery assessment](docs/rendering-recovery-assessment.md).

## Contributors and testers

Thank you to everyone who contributes readable code, reports reproducible failures,
checks background clients after desktop interruptions, or improves these docs.
The project's actual-client acceptance also depends on manual testing: native
synthetic tests cannot certify every retail client or desktop configuration.
