#pragma once

#include <SDL3/SDL_dialog.h>
#include <string>

namespace FileDialog
{
    // Returns the selected path, or empty string if the user cancelled.
    // Pass nullptr for filters/nFilters to use the default JSON preset filter.
    std::string Open(const char* Title = "Open",
                     const SDL_DialogFileFilter* Filters  = nullptr,
                     int                         NFilters = 0);

    std::string Save(const char* Title = "Save",
                     const char* DefaultPath = nullptr,
                     const SDL_DialogFileFilter* Filters  = nullptr,
                     int                         NFilters = 0);
}
