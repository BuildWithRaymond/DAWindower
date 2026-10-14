#include "imports.h"
#include <cstring>

namespace daw {
namespace {
DWORD exchange(void** slot, void* from, void* to) {
    DWORD previous = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &previous))
        return GetLastError();
    const bool changed = InterlockedCompareExchangePointer(slot, to, from) == from;
    DWORD unused = 0;
    if (!VirtualProtect(slot, sizeof(void*), previous, &unused)) {
        const auto error = GetLastError();
        if (changed)
            InterlockedCompareExchangePointer(slot, from, to);
        return error;
    }
    return changed ? ERROR_SUCCESS : ERROR_INVALID_STATE;
}
} // namespace

DWORD patchImport(HMODULE image,
                  const char* dll,
                  const char* name,
                  void* expected,
                  void* replacement,
                  std::vector<ImportPatch>& installed) {
    auto base = reinterpret_cast<BYTE*>(image);
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return ERROR_BAD_EXE_FORMAT;
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
        return ERROR_BAD_EXE_FORMAT;
    const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!directory.VirtualAddress || directory.VirtualAddress >= nt->OptionalHeader.SizeOfImage)
        return ERROR_PROC_NOT_FOUND;
    const auto descriptors =
        reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress);
    for (DWORD i = 0; i < directory.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR) && descriptors[i].Name;
         ++i) {
        const auto& d = descriptors[i];
        if (_stricmp(reinterpret_cast<const char*>(base + d.Name), dll))
            continue;
        auto slots = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + d.FirstThunk);
        const auto names =
            d.OriginalFirstThunk
                ? reinterpret_cast<const IMAGE_THUNK_DATA32*>(base + d.OriginalFirstThunk)
                : nullptr;
        for (DWORD j = 0; slots[j].u1.Function; ++j) {
            if (names) {
                if (IMAGE_SNAP_BY_ORDINAL32(names[j].u1.Ordinal))
                    continue;
                const auto entry =
                    reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names[j].u1.AddressOfData);
                if (std::strcmp(reinterpret_cast<const char*>(entry->Name), name))
                    continue;
            } else if (reinterpret_cast<void*>(slots[j].u1.Function) != expected)
                continue;
            auto slot = reinterpret_cast<void**>(&slots[j].u1.Function);
            installed.push_back({slot, expected, replacement}); // Allocate before touching the IAT.
            const auto result = exchange(slot, expected, replacement);
            if (result)
                installed.pop_back();
            return result;
        }
    }
    return ERROR_PROC_NOT_FOUND;
}

DWORD restoreImports(std::vector<ImportPatch>& installed) {
    DWORD result = ERROR_SUCCESS;
    for (size_t i = installed.size(); i > 0; --i) {
        const auto& patch = installed[i - 1];
        const auto error = *patch.slot == patch.original
                               ? ERROR_SUCCESS
                               : exchange(patch.slot, patch.replacement, patch.original);
        if (error && !result)
            result = error;
        if (!error)
            installed.erase(installed.begin() + static_cast<std::ptrdiff_t>(i - 1));
    }
    return result;
}
} // namespace daw
