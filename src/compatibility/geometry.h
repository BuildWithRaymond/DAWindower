#pragma once
#include "imports.h"

namespace daw {
// Patch only the executable's imports, outside loader lock. Native calls from
// Windows, launchers and DAWindower's own frame remain outside these adapters.
DWORD installClientGeometryImports(HMODULE image, std::vector<ImportPatch>& patches);
} // namespace daw
