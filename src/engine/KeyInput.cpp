#include "engine/KeyInput.h"

#include <algorithm>
#include <vector>

namespace avs
{
namespace
{
// Keycodes that saw a key-down edge since the last BeginFrame. Small in practice
// (a handful of presses per frame at most), so a linear-scanned vector is fine.
std::vector<uint32_t> s_pressed;
uint32_t              s_last = 0;

// Keycodes currently held down. Persists across frames (only changed by key
// down/up and ClearHeld), so effects can poll hold state.
std::vector<uint32_t> s_held;
} // namespace

void KeyInput::BeginFrame()
{
    s_pressed.clear();
    s_last = 0;
}

void KeyInput::PushKeyDown(uint32_t keycode)
{
    if (keycode == 0)
        return;
    s_pressed.push_back(keycode);
    s_last = keycode;
}

bool KeyInput::WasKeyPressed(uint32_t keycode)
{
    if (keycode == 0)
        return false;
    for (uint32_t k : s_pressed)
        if (k == keycode)
            return true;
    return false;
}

uint32_t KeyInput::LastKeyPressed()
{
    return s_last;
}

void KeyInput::SetKeyDown(uint32_t keycode, bool down)
{
    if (keycode == 0)
        return;
    auto it = std::find(s_held.begin(), s_held.end(), keycode);
    if (down)
    {
        if (it == s_held.end())
            s_held.push_back(keycode);
    }
    else if (it != s_held.end())
    {
        s_held.erase(it);
    }
}

bool KeyInput::IsKeyDown(uint32_t keycode)
{
    if (keycode == 0)
        return false;
    return std::find(s_held.begin(), s_held.end(), keycode) != s_held.end();
}

void KeyInput::ClearHeld()
{
    s_held.clear();
}
} // namespace avs
