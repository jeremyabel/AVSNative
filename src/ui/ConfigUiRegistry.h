#pragma once

#include <functional>
#include <string>
#include <unordered_map>

class Effect;

// Per-effect config UI draw function. Receives the selected effect; bespoke files
// cast it to the concrete type and draw against ConfigRef().
using EffectUiDraw = std::function<void(Effect*)>;

// Maps an effect's display name (== Effect::GetDescriptor().Name == EffectName())
// to its bespoke draw function.
class ConfigUiRegistry
{
public:
    void Register(const std::string& name, EffectUiDraw draw);
    const EffectUiDraw* Find(const std::string& name) const;

private:
    std::unordered_map<std::string, EffectUiDraw> m_draws;
};

// Descriptor-driven auto-generated UI. Used as the body of effects that don't need
// a custom layout (their UI file just forwards here), and as a safety fallback.
void DrawDefault(Effect* effect);

// Central explicit registration of every per-effect UI file. Mirrors the explicit
// effect registration in Engine.cpp (static-initializer self-registration would be
// stripped from the static lib).
void RegisterAllEffectUis(ConfigUiRegistry& reg);
