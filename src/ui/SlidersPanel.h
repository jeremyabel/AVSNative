#pragma once

class Engine;

// Dockable "Sliders" panel: edits the preset-global slider list on the Engine.
// Each row is laid out as [name][min][slider][max][remove]. `open` is the
// show/hide flag owned by App (bound to the View → Sliders menu toggle).
class SlidersPanel
{
public:
    static void Render(Engine& engine, bool* open);
};
