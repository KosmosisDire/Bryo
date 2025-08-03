#pragma once

// --- NEW: Platform-specific export/import macros ---
#ifdef _WIN32
#ifdef MYCELIUM_BUILD_DLL // Not used by us, but good practice
#define MYCELIUM_API __declspec(dllexport)
#else
// For an executable exporting symbols, we also use dllexport.
// For a static library, this would be empty.
#define MYCELIUM_API __declspec(dllexport)
#endif
#else // GCC, Clang
#define MYCELIUM_API __attribute__((visibility("default")))
#endif
// --- END NEW ---

#include <string>
#include <vector>
#include <stdexcept> // For std::runtime_error

// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#include <sstream> // For std::wostringstream
// For MAX_PATH, though we'll use a dynamic buffer

inline bool launchDebugger()
{
    // Get System directory, typically c:\windows\system32
    std::wstring systemDir(MAX_PATH + 1, '\0');
    UINT nChars = GetSystemDirectoryW(&systemDir[0], systemDir.length());
    if (nChars == 0)
        return false; // failed to get system directory
    systemDir.resize(nChars);

    // Get process ID and create the command line
    DWORD pid = GetCurrentProcessId();
    std::wostringstream s;
    s << systemDir << L"\\vsjitdebugger.exe -p " << pid;
    std::wstring cmdLine = s.str();

    // Start debugger process
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(NULL, &cmdLine[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        return false;

    // Close debugger process handles to eliminate resource leak
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    // Wait for the debugger to attach
    while (!IsDebuggerPresent())
        Sleep(100);

    // Stop execution so the debugger can take over
    DebugBreak();
    return true;
}

#elif defined(__linux__)
#include <unistd.h> // For readlink
#include <limits.h> // For PATH_MAX (as a starting guess)
#elif defined(__APPLE__) || defined(__MACH__)
#include <mach-o/dyld.h> // For _NSGetExecutablePath
#include <limits.h>      // For PATH_MAX (as a starting guess)
#else
#error "Unsupported platform: Cannot determine executable path."
#endif

inline std::string getExecutablePath()
{
#ifdef _WIN32
    std::vector<char> buffer(MAX_PATH);
    DWORD pathLen = 0;

    // Loop to handle cases where the buffer might be too small
    // (though MAX_PATH is usually sufficient for the executable itself)
    while (true)
    {
        pathLen = GetModuleFileNameA(NULL, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (pathLen == 0)
        {
            throw std::runtime_error("Failed to get executable path (GetModuleFileNameA failed with error " + std::to_string(GetLastError()) + ")");
        }
        if (pathLen < buffer.size())
        {
            // Success, the buffer was large enough
            return std::string(buffer.data(), pathLen);
        }
        // Buffer was too small, double its size and try again
        // This case is rare for GetModuleFileName with NULL module if MAX_PATH is used initially,
        // but good practice for robust dynamic buffer handling.
        buffer.resize(buffer.size() * 2);
        if (buffer.size() > 32767)
        { // Arbitrary limit to prevent infinite loop with very long paths
            throw std::runtime_error("Executable path too long or failed to allocate buffer.");
        }
    }

#elif defined(__linux__)
    std::vector<char> buffer(PATH_MAX);
    ssize_t len;

    while (true)
    {
        len = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1); // -1 for null terminator
        if (len == -1)
        {
            throw std::runtime_error("Failed to get executable path (readlink failed)");
        }
        if (static_cast<size_t>(len) < buffer.size() - 1)
        {
            buffer[len] = '\0'; // Null-terminate
            return std::string(buffer.data());
        }
        // Buffer was too small, double its size and try again
        buffer.resize(buffer.size() * 2);
        if (buffer.size() > 32767 * 2)
        { // Some reasonable upper limit
            throw std::runtime_error("Executable path too long or failed to allocate buffer for readlink.");
        }
    }

#elif defined(__APPLE__) || defined(__MACH__)
    // First, get the required buffer size
    uint32_t bufsize = 0;
    if (_NSGetExecutablePath(nullptr, &bufsize) != -1)
    {
        // This shouldn't happen: -1 is expected when buffer is nullptr to get size
        throw std::runtime_error("Failed to get executable path buffer size (unexpected _NSGetExecutablePath behavior)");
    }

    std::vector<char> buffer(bufsize);
    if (_NSGetExecutablePath(buffer.data(), &bufsize) == 0)
    {
        // The function null-terminates the string if the buffer is large enough.
        return std::string(buffer.data());
    }
    else
    {
        // This could happen if the path length changed between calls, though unlikely.
        // Or if there was another error.
        throw std::runtime_error("Failed to get executable path (_NSGetExecutablePath failed on second attempt)");
    }

#else
    // This part should ideally be caught by the #error directive above,
    // but as a fallback for compilers that might not strictly enforce it.
    return ""; // Or throw, depending on desired behavior for unsupported platforms
#endif
}

// Optional: A helper to get the directory of the executable
inline std::string getExecutableDir()
{
    std::string exePath = getExecutablePath();
    size_t lastSlash = exePath.find_last_of("/\\");
    if (lastSlash != std::string::npos)
    {
        return exePath.substr(0, lastSlash);
    }
    return "."; // Or empty string, or throw, if path is just "executable.exe"
}