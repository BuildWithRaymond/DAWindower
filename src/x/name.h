#pragma once
#include <windows.h>
#include <cstddef>

namespace daw::x {
struct ClientIdentity {
    DWORD timestamp, imageSize, entryRva;
};

bool supportedClient(ClientIdentity identity) noexcept;
bool decodeName(const char* bytes, size_t count, wchar_t (&name)[13]) noexcept;

class CharacterName {
    const BYTE* image = nullptr;

  public:
    CharacterName() noexcept;
    bool read(wchar_t (&name)[13]) const noexcept;
};
} // namespace daw::x
