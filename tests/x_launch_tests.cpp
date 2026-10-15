#include "x/launch.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <string>

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s (Win32=%lu)\n", what, GetLastError());
        std::exit(1);
    }
}

int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    wchar_t self[32768]{};
    check(GetModuleFileNameW(nullptr, self, 32768) != 0, "fixture executable directory");
    std::wstring directory(self);
    directory.resize(directory.find_last_of(L'\\'));
    auto root = directory + L"\\X launch fixture " + std::to_wstring(GetCurrentProcessId()) +
                L" \u6f22\u5b57";
    check(CreateDirectoryW(root.c_str(), nullptr) != FALSE,
          "create isolated Unicode fixture folder");
    auto executable = root + L"\\Darkages.exe";
    auto dll = root + L"\\DAWindowerX.dll";
    auto recordPath = root + L"\\x-fixture-result.bin";
    check(CopyFileW((directory + L"\\x_launch_fixture.exe").c_str(), executable.c_str(), TRUE) !=
              FALSE,
          "copy synthetic game fixture");
    check(CopyFileW((directory + L"\\DAWindowerX.dll").c_str(), dll.c_str(), TRUE) != FALSE,
          "copy one-DLL fixture package");
    const auto launched = daw::x::launchClient(executable.c_str(), dll.c_str());
    if (launched.error)
        std::fprintf(stderr,
                     "launch error=%lu stage=%d\n",
                     launched.error,
                     static_cast<int>(launched.stage));
    check(launched.error == ERROR_SUCCESS && launched.processId != 0,
          "suspended child loads X successfully before resume");
    DWORD record[3]{}, read = 0;
    bool received = false;
    const auto deadline = GetTickCount64() + 30000;
    while (!received && GetTickCount64() < deadline) {
        auto file = CreateFileW(recordPath.c_str(),
                                GENERIC_READ,
                                FILE_SHARE_READ,
                                nullptr,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            received =
                ReadFile(file, record, sizeof(record), &read, nullptr) && read == sizeof(record);
            CloseHandle(file);
        }
        if (!received)
            Sleep(20);
    }
    check(received && record[0] == launched.processId && record[1] == 1 && record[2] == 1,
          "fresh child initializes graphics/frame with zero initializer calls");
    check(DeleteFileW(recordPath.c_str()) != FALSE, "reset private fixture record");
    std::wstring command = L"\"" + executable + L"\" --ui-parent";
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION parent{};
    check(CreateProcessW(executable.c_str(),
                         command.data(),
                         nullptr,
                         nullptr,
                         FALSE,
                         CREATE_NO_WINDOW,
                         nullptr,
                         root.c_str(),
                         &startup,
                         &parent) != FALSE,
          "start own UI launch fixture");
    // The worker alone may use its full 15-second loader budget. Include the
    // parent/child's native graphics setup and scheduling on a busy desktop.
    check(WaitForSingleObject(parent.hProcess, 45000) == WAIT_OBJECT_0,
          "UI launch fixture exits within bound");
    DWORD parentCode = 1;
    GetExitCodeProcess(parent.hProcess, &parentCode);
    CloseHandle(parent.hThread);
    CloseHandle(parent.hProcess);
    if (parentCode)
        std::fprintf(stderr, "UI fixture exit=%lu\n", parentCode);
    check(parentCode == 0, "title-bar New client starts child with X injected");
    check(GetFileAttributesW((root + L"\\x-ui-parent-result.bin").c_str()) !=
              INVALID_FILE_ATTRIBUTES,
          "parent stays responsive while its launch worker runs");
    auto missing = root + L"\\missing.dll";
    const auto failed = daw::x::launchClient(executable.c_str(), missing.c_str());
    check(failed.error != ERROR_SUCCESS && failed.processId != 0,
          "failed injection is reported for newly created child");
    auto process = OpenProcess(SYNCHRONIZE, FALSE, failed.processId);
    check(!process || WaitForSingleObject(process, 0) == WAIT_OBJECT_0,
          "failed suspended child cleaned up without ever running game entry point");
    if (process)
        CloseHandle(process);
    check(daw::x::launchClient(L"missing.exe", dll.c_str()).error != ERROR_SUCCESS,
          "missing executable rejected");
    // The successful fixture exits itself. Its image may remain briefly mapped.
    for (unsigned i = 0; i < 100 && !DeleteFileW(executable.c_str()); ++i)
        Sleep(20);
    DeleteFileW(dll.c_str());
    DeleteFileW(recordPath.c_str());
    DeleteFileW((root + L"\\x-ui-parent-result.bin").c_str());
    RemoveDirectoryW(root.c_str());
    std::puts("Unicode suspended-start/injection and failure cleanup passed.");
}
