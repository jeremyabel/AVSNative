#pragma once

class Engine;

class Preset
{
public:

    // Loads a JSON preset file and populates the engine's effect chain.
    // Returns false if the file can't be opened or parsed.
    static bool Load(const char* Path, Engine& Engine);

    // Serialises the engine's current effect chain to a JSON preset file.
    // Returns false on write failure.
    static bool Save(const char* Path, Engine& Engine);
    
};
