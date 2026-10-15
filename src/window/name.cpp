#include "name.h"
#include <cstring>
#include <cwchar>

namespace daw {
namespace {
constexpr DWORD nameRva = 0x33d910, versionRva = 0x3505b8;

bool readMemory(const void* source, void* target, SIZE_T bytes) noexcept {
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(), source, target, bytes, &copied) &&
           copied == bytes;
}
} // namespace

bool supportedClient(ClientIdentity id) noexcept {
    // Installed official 7.41 executable, SHA256 documented in x-edition.md.
    // These layout identifiers are followed by an instruction/address guard.
    return id.timestamp == 0x56fb6f2d && id.imageSize == 0x506000 && id.entryRva == 0x22b2d9;
}

bool decodeName(const char* bytes, size_t count, wchar_t (&name)[13]) noexcept {
    name[0] = 0;
    if (!bytes || !count || count > 13)
        return false;
    wchar_t decoded[13]{};
    for (size_t i = 0; i < count; ++i) {
        const auto c = static_cast<unsigned char>(bytes[i]);
        if (!c) {
            if (!i)
                return false;
            std::wmemcpy(name, decoded, i + 1);
            return true;
        }
        if (i >= 12 || !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
            return false;
        decoded[i] = c;
    }
    return false;
}

CharacterName::CharacterName() noexcept {
    const auto base = reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS32 nt{};
    if (!base || !readMemory(base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew <= 0 || dos.e_lfanew > 0x100000 ||
        !readMemory(base + dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
        nt.FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        !supportedClient({nt.FileHeader.TimeDateStamp,
                          nt.OptionalHeader.SizeOfImage,
                          nt.OptionalHeader.AddressOfEntryPoint}))
        return;
    BYTE instruction[5]{};
    DWORD nameAddress = 0;
    if (!readMemory(base + 0x22591, instruction, sizeof(instruction)) || instruction[0] != 0x68)
        return;
    std::memcpy(&nameAddress, instruction + 1, sizeof(nameAddress));
    if (nameAddress == reinterpret_cast<DWORD>(base + nameRva))
        image = base;
}

bool CharacterName::read(wchar_t (&name)[13]) const noexcept {
    name[0] = 0;
    WORD version = 0;
    char first[13]{}, second[13]{};
    // Copy twice instead of dereferencing mutable client memory or trusting a
    // buffer while a login/relogin is overwriting it. Never write game memory.
    return image && readMemory(image + versionRva, &version, sizeof(version)) && version == 741 &&
           readMemory(image + nameRva, first, sizeof(first)) &&
           readMemory(image + nameRva, second, sizeof(second)) &&
           std::memcmp(first, second, sizeof(first)) == 0 && decodeName(first, sizeof(first), name);
}
} // namespace daw
