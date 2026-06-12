#include "engine/KeyInput.h"

#include <vector>

namespace avs
{
namespace
{
// Keycodes that saw a key-down edge since the last BeginFrame. Small in practice
// (a handful of presses per frame at most), so a linear-scanned vector is fine.
std::vector<uint32_t> s_pressed;
uint32_t              s_last = 0;
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
} // namespace avs
