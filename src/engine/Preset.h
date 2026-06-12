#pragma once

#include <memory>

class Engine;
class Effect;

class Preset
{
public:

    // Loads a JSON preset file and populates the engine's effect chain.
    // Returns false if the file can't be opened or parsed.
    static bool Load(const char* Path, Engine& Engine);

    // Serialises the engine's current effect chain to a JSON preset file.
    // Returns false on write failure.
    static bool Save(const char* Path, Engine& Engine);

    // Deep-clones an effect: a fresh instance of the same type with all parameters
    // (via Serialize/Deserialize), raw image/GIF assets (CollectAssets/ApplyAsset),
    // and any inner chain (EffectList) recursively copied. Returns nullptr if the
    // type isn't registered. The clone gets fresh runtime identity (no lock/id).
    static std::unique_ptr<Effect> CloneEffect(Engine& Engine, Effect& Src);

};
