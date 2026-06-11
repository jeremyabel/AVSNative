#include "FileDialog.h"

#include <SDL3/SDL.h>
#include <atomic>

struct DialogResult
{
    std::atomic<bool> done{false};
    std::string       path;
};

static void SDLCALL OnResult(void* userdata, const char* const* filelist, int /*filter*/)
{
    auto* r = static_cast<DialogResult*>(userdata);
    if (filelist && filelist[0])
        r->path = filelist[0];
    r->done.store(true, std::memory_order_release);
}

// Open accepts both the .avsz bundle and legacy asset-less .json presets.
static const SDL_DialogFileFilter k_openFilters[] = {
    { "AVS Presets", "avsz;json" },
    { "All Files",   "*"         },
};

// Save always writes the .avsz bundle.
static const SDL_DialogFileFilter k_saveFilters[] = {
    { "AVS Preset", "avsz" },
    { "All Files",  "*"    },
};

static const SDL_DialogFileFilter k_mp3Filters[] = {
    { "MP3 Audio", "mp3" },
    { "All Files", "*"   },
};

std::string FileDialog::Open(const char* /*Title*/,
                             const SDL_DialogFileFilter* Filters,
                             int NFilters)
{
    if (!Filters || NFilters == 0)
    {
        Filters  = k_openFilters;
        NFilters = 2;
    }

    DialogResult r;
    SDL_ShowOpenFileDialog(OnResult, &r, nullptr, Filters, NFilters, nullptr, false);
    while (!r.done.load(std::memory_order_acquire))
    {
        SDL_PumpEvents();
        SDL_Delay(10);
    }
    return r.path;
}

std::string FileDialog::Save(const char* /*Title*/,
                             const char* DefaultPath,
                             const SDL_DialogFileFilter* Filters,
                             int NFilters)
{
    if (!Filters || NFilters == 0)
    {
        Filters  = k_saveFilters;
        NFilters = 2;
    }

    DialogResult r;
    SDL_ShowSaveFileDialog(OnResult, &r, nullptr, Filters, NFilters, DefaultPath);
    while (!r.done.load(std::memory_order_acquire))
    {
        SDL_PumpEvents();
        SDL_Delay(10);
    }
    return r.path;
}
