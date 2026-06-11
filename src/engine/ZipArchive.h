#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Minimal, dependency-free ZIP reader/writer that ONLY uses the "store" method
// (no compression). Produces/consumes standard ZIP files openable by any unzip
// utility (Windows Explorer, `unzip`, etc.). Used for the .avsz preset bundle
// format (preset.json + raw asset files under assets/).
//
// Intentionally self-contained (own CRC32) to avoid linking miniz's archive APIs,
// which are disabled in the vendored copies and would risk duplicate mz_* symbols.
namespace ZipArchive
{
    struct Entry
    {
        std::string          Name;   // forward-slash path, e.g. "assets/cat.gif"
        std::vector<uint8_t> Bytes;  // raw, unmodified file content
    };

    // Write a standard store-only zip at Path. Entry order is preserved.
    // Returns false on any I/O failure.
    bool Write(const char* Path, const std::vector<Entry>& Entries);

    // Read a zip at Path into entries (in central-directory order). Stored entries
    // only — any entry with a non-store compression method is skipped. Returns
    // false if the file can't be opened or isn't a valid zip.
    bool Read(const char* Path, std::vector<Entry>& OutEntries);

    // Look up one entry's bytes by exact name. Returns nullptr if absent.
    const std::vector<uint8_t>* Find(const std::vector<Entry>& Entries,
                                     const std::string& Name);
}
