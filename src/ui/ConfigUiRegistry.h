#pragma once

#include <functional>
#include <string>
#include <unordered_map>

class Effect;

// Per-effect config UI draw function. Receives the selected effect; each UI file
// casts it to the concrete type and binds ImGui widgets directly to its public
// config members.
using EffectUiDraw = std::function<void(Effect*)>;

// Maps an effect's display name (== Effect::Name()) to its bespoke draw function.
class ConfigUiRegistry
{
public:
    void Register(const std::string& name, EffectUiDraw draw);
    const EffectUiDraw* Find(const std::string& name) const;

private:
    std::unordered_map<std::string, EffectUiDraw> m_draws;
};

// Central explicit registration of every per-effect UI file. Mirrors the explicit
// effect registration in Engine.cpp (static-initializer self-registration would be
// stripped from the static lib).
void RegisterAllEffectUis(ConfigUiRegistry& reg);
