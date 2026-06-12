#include "engine/Engine.h"
#include "ui/App.h"

#if defined(__APPLE__)
#include <cstdlib>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <string>
#include <unistd.h>
#include <vector>

// On macOS, bgfx's Vulkan backend dlopen()s the bare name "libMoltenVK.dylib".
// Apple Silicon Homebrew installs it under /opt/homebrew/lib, which is NOT on
// dyld's default search path, so the dlopen fails, bgfx silently falls back to
// the Metal renderer, and every (SPIRV-only) shader create fails with a fatal
// "Failed to create Vertex shader" at startup.
//
// DYLD_LIBRARY_PATH is only honoured by dyld at process launch — setting it at
// runtime does not affect later dlopen() searches — so if MoltenVK is installed
// somewhere off-path we prepend its directory to DYLD_LIBRARY_PATH and re-exec
// ourselves once. A sentinel env var prevents an exec loop.
static void EnsureMoltenVKOnDyldPath(char* argv[])
{
    if (getenv("AVS_DYLD_REEXEC"))
        return;  // Already re-exec'd once; don't loop.

    if (void* h = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL))
    {
        dlclose(h);  // Already findable via the default/inherited search path.
        return;
    }

    static const char* kDirs[] = {
        "/opt/homebrew/lib",                    // Apple Silicon Homebrew
        "/opt/homebrew/opt/molten-vk/lib",
        "/usr/local/lib",                       // Intel Homebrew
        "/usr/local/opt/molten-vk/lib",
    };
    const char* found = nullptr;
    for (const char* dir : kDirs)
    {
        std::string p = std::string(dir) + "/libMoltenVK.dylib";
        if (access(p.c_str(), R_OK) == 0) { found = dir; break; }
    }
    if (!found)
        return;  // Not installed — App's renderer guard prints the `brew` hint.

    std::string newPath = found;
    if (const char* existing = getenv("DYLD_LIBRARY_PATH"))
        newPath += std::string(":") + existing;
    setenv("DYLD_LIBRARY_PATH", newPath.c_str(), 1);
    setenv("AVS_DYLD_REEXEC", "1", 1);

    char exePath[4096];
    uint32_t size = sizeof(exePath);
    if (_NSGetExecutablePath(exePath, &size) == 0)
        execv(exePath, argv);
    else
        execv(argv[0], argv);  // Fallback; harmless if it fails.
}
#endif

int main(int argc, char* argv[])
{
#if defined(__APPLE__)
    EnsureMoltenVKOnDyldPath(argv);
#endif
    Engine engine;
    App app(engine);
    app.Run(argc > 1 ? argv[1] : nullptr);
    return 0;
}
