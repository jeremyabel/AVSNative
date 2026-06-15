#pragma once

// Reference-based JSON readers for effect Deserialize() implementations.
// Each Read* assigns into the passed member only when the key exists with a
// usable JSON type; otherwise the member keeps its current value, so the
// in-class member initializers remain the single source of defaults.

#include "engine/ColorList.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace JsonUtil
{
using Rgb = std::array<uint8_t, 3>;

inline void ReadFloat(const nlohmann::json& j, const char* key, float& v)
{
    if (auto it = j.find(key); it != j.end() && it->is_number())
        v = it->get<float>();
}

inline void ReadInt(const nlohmann::json& j, const char* key, int& v)
{
    if (auto it = j.find(key); it != j.end() && it->is_number())
        v = it->get<int>();
}

inline void ReadBool(const nlohmann::json& j, const char* key, bool& v)
{
    if (auto it = j.find(key); it != j.end() && it->is_boolean())
        v = it->get<bool>();
}

inline void ReadString(const nlohmann::json& j, const char* key, std::string& v)
{
    if (auto it = j.find(key); it != j.end() && it->is_string())
        v = it->get<std::string>();
}

inline void ReadColor(const nlohmann::json& j, const char* key, Rgb& v)
{
    auto it = j.find(key);
    if (it == j.end() || !it->is_array() || it->size() != 3)
        return;
    v = { (uint8_t)(*it)[0].get<int>(),
          (uint8_t)(*it)[1].get<int>(),
          (uint8_t)(*it)[2].get<int>() };
}

// Ignores empty arrays so the "at least one color" invariant holds.
inline void ReadColors(const nlohmann::json& j, const char* key, ColorList& v)
{
    auto it = j.find(key);
    if (it == j.end() || !it->is_array() || it->empty())
        return;
    std::vector<Rgb> out;
    out.reserve(it->size());
    for (const nlohmann::json& c : *it)
    {
        if (!c.is_array() || c.size() != 3)
            continue;
        out.push_back({ (uint8_t)c[0].get<int>(),
                        (uint8_t)c[1].get<int>(),
                        (uint8_t)c[2].get<int>() });
    }
    if (!out.empty())
        v.Entries = std::move(out);
}

inline nlohmann::json ColorToJson(const Rgb& c)
{
    return { (int)c[0], (int)c[1], (int)c[2] };
}

inline nlohmann::json ColorsToJson(const ColorList& cs)
{
    nlohmann::json arr = nlohmann::json::array();
    for (const Rgb& c : cs.Entries)
        arr.push_back(ColorToJson(c));
    return arr;
}
} // namespace JsonUtil
