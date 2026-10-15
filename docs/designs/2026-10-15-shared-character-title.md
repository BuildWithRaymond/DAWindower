# Shared character title

Historical design, superseded by [window title ownership](2026-10-15-window-title-ownership.md).
The memory reader described below has been removed from both editions.

The standard edition will show the same `Dark Ages · CharacterName` title as X,
as requested on October 15, 2026. When the existing reader cannot obtain a valid
name, both editions show `Dark Ages`.

Move the existing bounded name reader into `src/window/` and build it into both
editions. Use the same caption title update and one-second UI timer for both.
Keep new-client launch handling and the two extra caption controls conditional
on the X build. Retain the existing supported-client checks and read-only access.

Extend the shared caption contract to check initial fallback, recovery after a
host title change and independent window titles. Keep name decoding and build
guards in a shared unit contract. Build and test both editions and package the
standard DLL in a fresh candidate directory. Actual login timing remains a
separate client acceptance check.

## Verification

The new standard title fallback assertion failed against the previous behavior
before implementation. Both Release builds now pass their full suites on the
interactive Windows desktop: 22 standard contracts and 23 X contracts. Formatting,
documentation links and whitespace checks pass.

The standard candidate is `dist/0.2.3-character-title/DAWindower.dll`. Its package
check confirms one x86 DLL with only system imports and the existing export ABI.
The sandbox's separate desktop caused recovery/pixel failures during the first
test run; those checks passed on the interactive desktop. Live login-name timing
was not retested for this change.
