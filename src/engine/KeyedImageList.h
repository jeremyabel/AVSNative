#pragma once

#include "engine/Effect.h"   // PresetAsset

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

// ── Per-instance keyed image list ─────────────────────────────────────────────
// A variable-length list of images, each bound to a keyboard key. Owned as a member
// by an effect (Texer2, ImageGrid) — there is NO global/shared list; two effect
// instances hold independent lists and independent Selected state.
//
// This class owns the bookkeeping that's identical across effects: the
// {raw bytes, name, keycode} entries, the currently-selected index, JSON
// serialization, preset-bundle asset round-trip, and key-driven selection. The
// owning effect still does its own GPU decode from the selected entry's raw bytes.
//
// Serialization piggy-backs on the existing preset asset mechanism (Preset.cpp):
// each entry is a top-level config string key "imageN" holding its bundle ref, so
// CollectAssets()/ApplyAsset() route bytes per index. "imageN" never collides with
// the single-image "imageData" key (that has non-digits after "image"). The keymap
// is an int array "imageKeys" (ignored by the string-only asset resolver).
struct KeyedImage
{
    std::vector<uint8_t> Raw;          // original bytes (for bundling + decode)
    std::string          Name;         // original filename / bundle ref
    uint32_t             Keycode = 0;  // SDL keycode bound to this image (0 = unbound)
};

class KeyedImageList
{
public:
    std::vector<KeyedImage> Images;
    int                     Selected = 0;

    // Writes imageCount, image0..N (names), imageKeys[], selectedImage into `j`.
    void Serialize(nlohmann::json& j) const;

    // Rebuilds Images from `j` (Raw left empty — bytes arrive via ApplyAsset), sets
    // each Keycode/Name, and restores Selected.
    void Deserialize(const nlohmann::json& j);

    // Appends a PresetAsset {"imageN", Name, Raw} for every entry that has bytes.
    void CollectAssets(std::vector<PresetAsset>& out) const;

    // If `key` is "imageN", stores bytes/name into entry N (growing the list if
    // needed) and returns true; returns false for any other key (e.g. "imageData").
    bool ApplyAsset(const std::string& key, std::string name, std::vector<uint8_t> bytes);

    // Polls KeyInput: if a mapped key saw a key-DOWN edge this frame, switches
    // Selected to that entry. Returns true if Selected changed.
    bool UpdateSelection();

    // Clamps Selected into [0, size); 0 when empty.
    void ClampSelected();
};
