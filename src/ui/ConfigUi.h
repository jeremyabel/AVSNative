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
struct ColorList;

namespace ConfigUi
{
enum class Lang
{
    Glsl,
    Lua,
};

// Namespaces subsequent CodeEditor() calls to a specific effect instance so the
// same editor id (e.g. "texer2.initCode") used by two effects — even of the same
// type, shown in two panels at once — gets independent editor state. Call before
// drawing an effect's body; pass nullptr to clear the scope.
void SetEditorScope(const void* effect);

// Renders a syntax-highlighted code editor identified by `id` (stable per editor,
// within the current SetEditorScope). `text` is read and written in place. Returns
// true if the text changed this frame. If `text` is changed programmatically between
// frames (e.g. Movement swapping its default code), the editor re-syncs to it.
bool CodeEditor(const char* id, std::string& text, Lang lang, float height = 150.0f);

// Shows the effect's compile/runtime error for the named code block (via
// Effect::GetScriptError) as red text, or nothing if there's no error. Call right
// after a CodeEditor() for the matching block.
void ScriptError(Effect* effect, const char* paramName);

// Drops editor instances that weren't drawn this frame. Call once after all config
// panels are rendered; bounds the editor map as effects are added/removed/unlocked.
void EndFramePrune();

// Clears all editor instances (e.g. on full chain clear).
void ResetEditors();

// uint8 RGB color picker. Returns true if changed.
bool ColorEdit(const char* label, std::array<uint8_t, 3>& c);

// Editable color list: one compact ColorEdit per entry plus remove ("x", guarded
// so at least one color always remains) and add ("+") buttons. Returns true if
// any color or the list itself changed.
bool ColorsEdit(const char* id, ColorList& list);

// Opens an image file picker; on selection reads the raw bytes and hands them to
// the effect via Effect::ApplyAsset(configKey, basename, bytes) — no base64. The
// raw bytes are bundled into the preset on save. Returns true if an image loaded.
bool PickImageInto(Effect* effect, const char* configKey);

// Press-to-capture key-binding button for a single SDL keycode (0 = unbound).
// `owner`+`index` scope the armed capture so only one button captures at a time and
// it can never write another binding. Left-click arms capture (the next key pressed
// binds); right-click clears the binding. Shows the bound key's name via
// SDL_GetKeyName. Returns true if the keycode changed this frame.
bool KeyCaptureButton(const char* id, const void* owner, int index, uint32_t& keycode);

// Editor for a keyed image list (per-instance image→key mappings). Per entry: a
// "Load…" button (with loaded/empty hint), a "Set key" press-to-capture button
// showing the bound key's name, and a remove button; plus an "Add image" button.
// The current selection is highlighted. Images load via Effect::ApplyAsset under
// the per-index key "imageN". Returns true if the active image must be reloaded
// (entry added/removed, image loaded, or selection changed); the caller then calls
// the effect's LoadSelected().
bool KeyedImageListEditor(Effect* effect, KeyedImageList& list);
} // namespace ConfigUi
