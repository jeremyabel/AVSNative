#pragma once

// Small shared helpers for per-effect config UIs. Everything else in a bespoke UI
// file is plain ImGui — these only cover the genuinely fiddly cases: hosting a
// persistent code-editor instance, and converting uint8 RGB <-> ImGui's float[3].

#include <array>
#include <cstdint>
#include <string>

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
} // namespace ConfigUi
