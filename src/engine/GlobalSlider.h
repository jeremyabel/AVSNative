#pragma once

#include <string>

// A named, preset-global slider. The editable list lives on Engine; the UI edits
// it, Preset serializes it, and each frame Engine pushes the name→value pairs into
// LuaRuntime so scripts can read the current value via slider("name").
struct GlobalSlider
{
    std::string Name  = "slider";
    float       Min   = 0.0f;
    float       Max   = 1.0f;
    float       Value = 0.0f;
};
