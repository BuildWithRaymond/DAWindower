#include "mutex.h"
#include <cstdio>
#include <cstring>

namespace daw {
HANDLE WINAPI createClientMutex(LPSECURITY_ATTRIBUTES attributes, BOOL owner, LPCSTR name) {
    if (!name || std::strcmp(name, "Nexon.SingleInstance") != 0)
        return CreateMutexA(attributes, owner, name);
    // Verified singleton name in the inspected client. Preserve native ownership,
    // handles and error semantics while isolating independent client processes.
    char localName[96]{};
    sprintf_s(localName, "Nexon.SingleInstance.DAWindower.%lu", GetCurrentProcessId());
    return CreateMutexA(attributes, owner, localName);
}
} // namespace daw
