#include "x/name.h"
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cstring>

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

int main() {
    wchar_t name[13]{};
    check(daw::x::decodeName("Saraet", 7, name) && std::wcscmp(name, L"Saraet") == 0,
          "terminated character name is decoded");
    check(daw::x::decodeName("Abcdefghijkl", 13, name) && std::wcslen(name) == 12,
          "maximum twelve-letter name accepted");
    check(daw::x::decodeName("New\0oldletters", 13, name) && std::wcscmp(name, L"New") == 0,
          "relogin ignores bytes after first terminator");
    check(!daw::x::decodeName("", 1, name) && name[0] == 0, "empty buffer clears name");
    check(!daw::x::decodeName("Abcdefghijklm", 13, name) && name[0] == 0,
          "unterminated and oversized name rejected");
    check(!daw::x::decodeName("Bob\n", 5, name) && name[0] == 0,
          "control characters rejected without leaking previous title");
    check(!daw::x::decodeName(nullptr, 13, name), "unavailable buffer rejected");
    check(daw::x::supportedClient({0x56fb6f2d, 0x506000, 0x22b2d9}),
          "verified installed 7.41 layout recognized");
    check(!daw::x::supportedClient({0x56fb6f2e, 0x506000, 0x22b2d9}),
          "different executable timestamp cannot reuse name offset");
    check(!daw::x::supportedClient({0x56fb6f2d, 0x506001, 0x22b2d9}),
          "different executable layout cannot reuse name offset");
    daw::x::CharacterName reader;
    check(!reader.read(name) && name[0] == 0, "unknown host executable uses safe fallback");
    std::puts("Character-name bounds and build guard passed.");
}
