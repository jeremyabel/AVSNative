#pragma once

// Small shared helpers for per-effect config UIs. Everything else in a bespoke UI
// file is plain ImGui — these only cover the genuinely fiddly cases: hosting a
// persistent code-editor instance, and converting uint8 RGB <-> ImGui's float[3].

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class Effect;
class KeyedImageList;

namespace ConfigUi
{
enum class Lang
{
    Glsl,
    Lua,
};

// Renders a syntax-highlighted code editor identified by `id` (stable per editor).
// `text` is read and written in place. Returns true if the text changed this frame.
// If `text` is changed programmatically between frames (e.g. Movement swapping its
// default code), the editor re-syncs to it.
bool CodeEditor(const char* id, std::string& text, Lang lang, float height = 150.0f);

// Clears all editor instances. Call when the selected effect changes so stale
// editors don't leak between effects.
void ResetEditors();

// uint8 RGB color picker. Returns true if changed.
bool ColorEdit(const char* label, std::array<uint8_t, 3>& c);

// Editable color list: one compact ColorEdit per entry plus remove ("x", guarded
// so at least one color always remains) and add ("+") buttons. Returns true if
// any color or the list itself changed.
bool ColorsEdit(const char* id, std::vector<std::array<uint8_t, 3>>& colors);

// Opens an image file picker; on selection reads the raw bytes and hands them to
// the effect via Effect::ApplyAsset(configKey, basename, bytes) — no base64. The
// raw bytes are bundled into the preset on save. Returns true if an image loaded.
bool PickImageInto(Effect* effect, const char* configKey);

// Editor for a keyed image list (per-instance image→key mappings). Per entry: a
// "Load…" button (with loaded/empty hint), a "Set key" press-to-capture button
// showing the bound key's name, and a remove button; plus an "Add image" button.
// The current selection is highlighted. Images load via Effect::ApplyAsset under
// the per-index key "imageN". Returns true if the active image must be reloaded
// (entry added/removed, image loaded, or selection changed); the caller then calls
// the effect's LoadSelected().
bool KeyedImageListEditor(Effect* effect, KeyedImageList& list);
} // namespace ConfigUi
