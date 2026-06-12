#pragma once

class Engine;
class EffectChain;
struct EffectEntry;

class ConfigPanel
{
public:
    // Renders the shared "Properties" window for the selected effect (entry may be
    // nullptr). If the selected effect is locked it has its own panel, so this shows
    // a hint instead of duplicating its editors. engine provides context but isn't
    // mutated here.
    static void Render(Engine& engine, EffectEntry* selected);

    // Renders one dockable panel per locked effect in the tree (root + nested inner
    // chains). Closing a panel's window untoggles that effect's lock.
    static void RenderLockedPanels(Engine& engine, EffectChain& root);
};
