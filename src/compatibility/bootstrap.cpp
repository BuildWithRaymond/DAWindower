#include "bootstrap.h"
#include "runtime.h"
#include "mutex.h"
#include "dawindower.h"

namespace daw {
namespace {
struct Gate {
    void** slot;
    void* original;
    void* replacement;
};

Gate gates[2]{};

// No CRT string helpers, allocation, loader calls, application locks or worker.
bool equal(const char* a, const char* b, bool fold = false) noexcept {
    for (unsigned i = 0; i < 128; ++i) {
        auto x = a[i], y = b[i];
        if (fold) {
            if (x >= 'A' && x <= 'Z')
                x += 'a' - 'A';
            if (y >= 'A' && y <= 'Z')
                y += 'a' - 'A';
        }
        if (x != y)
            return false;
        if (!x)
            return true;
    }
    return false;
}

void** slotFor(HMODULE image, const char* library, const char* symbol) noexcept {
    auto base = reinterpret_cast<BYTE*>(image);
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        dos->e_lfanew > 0x100000)
        return nullptr;
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
        return nullptr;
    const auto size = nt->OptionalHeader.SizeOfImage;
    auto fits = [size](DWORD rva, size_t bytes) {
        return rva < size && bytes <= size - rva;
    };
    const auto dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress || !fits(dir.VirtualAddress, dir.Size))
        return nullptr;
    auto entries = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
    for (DWORD i = 0; i < dir.Size / sizeof(*entries) && entries[i].Name; ++i) {
        const auto& e = entries[i];
        if (!fits(e.Name, 128) ||
            !equal(reinterpret_cast<const char*>(base + e.Name), library, true))
            continue;
        if (!e.OriginalFirstThunk)
            return nullptr; // Do not guess symbols from bound addresses.
        for (DWORD j = 0; j < size / sizeof(IMAGE_THUNK_DATA32); ++j) {
            const auto offset = j * sizeof(IMAGE_THUNK_DATA32);
            if (!fits(e.OriginalFirstThunk, offset + sizeof(IMAGE_THUNK_DATA32)) ||
                !fits(e.FirstThunk, offset + sizeof(IMAGE_THUNK_DATA32)))
                return nullptr;
            auto name =
                reinterpret_cast<const IMAGE_THUNK_DATA32*>(base + e.OriginalFirstThunk) + j;
            if (!name->u1.AddressOfData)
                break;
            if (IMAGE_SNAP_BY_ORDINAL32(name->u1.Ordinal))
                continue;
            if (!fits(name->u1.AddressOfData, 130))
                return nullptr;
            auto entry =
                reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + name->u1.AddressOfData);
            if (equal(reinterpret_cast<const char*>(entry->Name), symbol))
                return reinterpret_cast<void**>(base + e.FirstThunk + offset);
        }
    }
    return nullptr;
}

DWORD swap(Gate& gate, void* from, void* to) noexcept {
    if (!gate.slot)
        return ERROR_SUCCESS;
    DWORD protection = 0;
    if (!VirtualProtect(gate.slot, sizeof(void*), PAGE_READWRITE, &protection))
        return GetLastError();
    const bool changed = InterlockedCompareExchangePointer(gate.slot, to, from) == from;
    DWORD ignored = 0;
    if (!VirtualProtect(gate.slot, sizeof(void*), protection, &ignored)) {
        const auto error = GetLastError();
        if (changed)
            InterlockedCompareExchangePointer(gate.slot, from, to);
        return error;
    }
    return changed ? ERROR_SUCCESS : ERROR_INVALID_STATE;
}

HRESULT WINAPI firstDraw(GUID* guid, IDirectDraw** output, IUnknown* outer) {
    if (output)
        *output = nullptr;
    const auto result = DAWindowerInitialize(nullptr);
    return result ? HRESULT_FROM_WIN32(result) : createDraw(guid, output, outer);
}

HANDLE WINAPI firstMutex(LPSECURITY_ATTRIBUTES attributes, BOOL owner, LPCSTR name) {
    // Kernel-only singleton adapter needs no renderer initialization. A client
    // may perform this check before its first window or graphics factory call.
    return createClientMutex(attributes, owner, name);
}
} // namespace

BOOL armBootstrap(HMODULE image) noexcept {
    gates[0] = {slotFor(image, "ddraw.dll", "DirectDrawCreate"),
                nullptr,
                reinterpret_cast<void*>(firstDraw)};
    if (!gates[0].slot)
        return TRUE; // Non-client tools can still use explicit API.
    gates[1] = {slotFor(image, "kernel32.dll", "CreateMutexA"),
                nullptr,
                reinterpret_cast<void*>(firstMutex)};
    for (auto& gate : gates) {
        if (!gate.slot)
            continue;
        gate.original = *gate.slot;
        if (swap(gate, gate.original, gate.replacement)) {
            disarmBootstrap();
            return FALSE;
        }
    }
    return TRUE;
}

DWORD validateBootstrap(void* directDraw, void* createMutex) noexcept {
    void* expected[]{directDraw, createMutex};
    for (unsigned i = 0; i < 2; ++i) {
        const auto& gate = gates[i];
        if (gate.slot && (gate.original != expected[i] ||
                          (*gate.slot != gate.replacement && *gate.slot != gate.original)))
            return ERROR_INVALID_STATE;
    }
    return ERROR_SUCCESS;
}

void* bootstrapTarget(unsigned index, void* fallback) noexcept {
    return gates[index].slot && *gates[index].slot == gates[index].replacement
               ? gates[index].replacement
               : fallback;
}

DWORD disarmBootstrap() noexcept {
    DWORD error = ERROR_SUCCESS;
    for (auto& gate : gates) {
        if (!gate.slot || *gate.slot == gate.original)
            continue;
        const auto result = swap(gate, gate.replacement, gate.original);
        if (!error)
            error = result;
    }
    return error;
}
} // namespace daw
