#pragma once

class Engine;
class Effect;

class ConfigPanel
{
public:
    // Renders property widgets for the selected effect (may be nullptr).
    // engine is used for context (e.g., window dimensions) but not mutated here.
    static void Render(Engine& engine, Effect* effect);
};
