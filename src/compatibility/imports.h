#pragma once
#include <windows.h>
#include <vector>

namespace daw {
// These vector-backed helpers run outside loader lock. bootstrap.cpp provides
// the separate fixed-storage attach path. Restoring a slot checks hook ownership.
struct ImportPatch {
    void** slot;
    void* original;
    void* replacement;
};

DWORD patchImport(HMODULE image,
                  const char* dll,
                  const char* name,
                  void* expected,
                  void* replacement,
                  std::vector<ImportPatch>& installed);
DWORD restoreImports(std::vector<ImportPatch>& installed);
} // namespace daw
