#pragma once

// Reflection layer for effect parameters.
//
// Each effect declares a plain `Config` struct (one member per parameter) and a
// single table of `Field`s built with the typed factories below. The table is the
// ONE place a parameter name string appears; it is bound to a config-struct member
// via a type-safe pointer-to-member. `ReflectedEffect<Config>` then implements
// GetDescriptor()/GetConfig()/SetConfig() generically by iterating that table, so
// effects no longer hand-write serialization.
//
// Side-effects (recompiling a shader/Lua block when a field changes) are handled by
// overriding OnConfigChanged(changedKeys).

#include "engine/Effect.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <vector>

// One reflected parameter: UI metadata (ParamDesc) plus type-erased JSON accessors
// bound to a specific member of a specific Config struct.
struct Field
{
    ParamDesc Desc;
    std::function<void(const void* cfg, nlohmann::json&)> Save;  // cfg.member -> j[Name]
    std::function<void(void* cfg, const nlohmann::json&)>  Load;  // j[Name]   -> cfg.member
};

// ── Factory helpers ───────────────────────────────────────────────────────────
// Each captures a pointer-to-member so the name string is written exactly once.

template <class C>
Field Range(float C::* m, std::string name, std::string label,
            float min, float max, float step = 0.01f)
{
    ParamDesc d{ std::move(name), std::move(label), ParamType::Range, min, max, step, {} };
    return {
        d,
        [m, key = d.Name](const void* cfg, nlohmann::json& j) {
            j[key] = static_cast<const C*>(cfg)->*m;
        },
        [m, key = d.Name, min, max](void* cfg, const nlohmann::json& j) {
            static_cast<C*>(cfg)->*m = std::clamp(j.at(key).get<float>(), min, max);
        },
    };
}

template <class C>
Field RangeI(int C::* m, std::string name, std::string label, int min, int max)
{
    ParamDesc d{ std::move(name), std::move(label), ParamType::Range,
                 (float)min, (float)max, 1.0f, {} };
    return {
        d,
        [m, key = d.Name](const void* cfg, nlohmann::json& j) {
            j[key] = static_cast<const C*>(cfg)->*m;
        },
        [m, key = d.Name, min, max](void* cfg, const nlohmann::json& j) {
            static_cast<C*>(cfg)->*m = std::clamp(j.at(key).get<int>(), min, max);
        },
    };
}

template <class C>
Field NumberI(int C::* m, std::string name, std::string label, int min, int max)
{
    ParamDesc d{ std::move(name), std::move(label), ParamType::Number,
                 (float)min, (float)max, 1.0f, {} };
    return {
        d,
        [m, key = d.Name](const void* cfg, nlohmann::json& j) {
            j[key] = static_cast<const C*>(cfg)->*m;
        },
        [m, key = d.Name, min, max](void* cfg, const nlohmann::json& j) {
            static_cast<C*>(cfg)->*m = std::clamp(j.at(key).get<int>(), min, max);
        },
    };
}

// One element of a fixed-size int[] config member, serialized under its own key.
template <class C, std::size_t N>
Field RangeIArr(int (C::* m)[N], int idx, std::string name, std::string label, int min, int max)
{
    ParamDesc d{ std::move(name), std::move(label), ParamType::Range,
                 (float)min, (float)max, 1.0f, {} };
    return {
        d,
        [m, idx, key = d.Name](const void* cfg, nlohmann::json& j) {
            j[key] = (static_cast<const C*>(cfg)->*m)[idx];
        },
        [m, idx, key = d.Name, min, max](void* cfg, const nlohmann::json& j) {
            (static_cast<C*>(cfg)->*m)[idx] = std::clamp(j.at(key).get<int>(), min, max);
        },
    };
}

template <class C>
Field Bool(bool C::* m, std::string name, std::string label)
{
    ParamDesc d{ std::move(name), std::move(label), ParamType::Bool, 0, 1, 1, {} };
    return {
        d,
        [m, key = d.Name](const void* cfg, nlohmann::json& j) {
            j[key] = static_cast<const C*>(cfg)->*m;
        },
        [m, key = d.Name](void* cfg, const nlohmann::json& j) {
            static_cast<C*>(cfg)->*m = j.at(key).get<bool>();
        },
    };
}

// Select stored as an integer index.
template <class C>
Field SelectI(int C::* m, std::string name, std::string label,
              std::vector<std::string> opts)
{
    const int hi = opts.empty() ? 0 : (int)opts.size() - 1;
    ParamDesc d{ std::move(name), std::move(label), ParamType::Select,
                 0, (float)hi, 1, std::move(opts) };
    return {
        d,
        [m, key = d.Name](const void* cfg, nlohmann::json& j) {
            j[key] = static_cast<const C*>(cfg)->*m;
        },
        [m, key = d.Name, hi](void* cfg, const nlohmann::json& j) {
            static_cast<C*>(cfg)->*m = std::clamp(j.at(key).get<int>(), 0, hi);
        },
    };
}

// Select stored as a string value (e.g. Movement "polar"/"cartesian").
template <class C>
Field SelectS(std::string C::* m, std::string name, std::string label,
              std::vector<std::string> opts)
{
    ParamDesc d{ std::move(name), std::move(label), ParamType::Select,
                 0, 0, 0, std::move(opts) };
    return {
        d,
        [m, key = d.Name](const void* cfg, nlohmann::json& j) {
            j[key] = static_cast<const C*>(cfg)->*m;
        },
        [m, key = d.Name](void* cfg, const nlohmann::json& j) {
            static_cast<C*>(cfg)->*m = j.at(key).get<std::string>();
        },
    };
}

// Single RGB color, stored as std::array<uint8_t,3>, serialized as [r,g,b] (0-255).
template <class C>
Field Color(std::array<uint8_t, 3> C::* m, std::string name, std::string label)
{
    ParamDesc d{ std::move(name), std::move(label), ParamType::Color, 0, 255, 1, {} };
    return {
        d,
        [m, key = d.Name](const void* cfg, nlohmann::json& j) {
            const auto& c = static_cast<const C*>(cfg)->*m;
            j[key] = { (int)c[0], (int)c[1], (int)c[2] };
        },
        [m, key = d.Name](void* cfg, const nlohmann::json& j) {
            const auto& src = j.at(key);
            if (src.is_array() && src.size() == 3) {
                auto& c = static_cast<C*>(cfg)->*m;
                c = { (uint8_t)src[0].get<int>(),
                      (uint8_t)src[1].get<int>(),
                      (uint8_t)src[2].get<int>() };
            }
        },
    };
}

// Color list, stored as std::vector<std::array<uint8_t,3>>, serialized as array of [r,g,b].
// Always keeps at least one color on load.
template <class C>
Field Colors(std::vector<std::array<uint8_t, 3>> C::* m, std::string name, std::string label)
{
    ParamDesc d{ std::move(name), std::move(label), ParamType::Colors, 0, 255, 1, {} };
    return {
        d,
        [m, key = d.Name](const void* cfg, nlohmann::json& j) {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& c : static_cast<const C*>(cfg)->*m)
                arr.push_back({ (int)c[0], (int)c[1], (int)c[2] });
            j[key] = arr;
        },
        [m, key = d.Name](void* cfg, const nlohmann::json& j) {
            const auto& src = j.at(key);
            if (!src.is_array()) return;
            auto& list = static_cast<C*>(cfg)->*m;
            list.clear();
            for (const auto& c : src)
                if (c.is_array() && c.size() == 3)
                    list.push_back({ (uint8_t)c[0].get<int>(),
                                     (uint8_t)c[1].get<int>(),
                                     (uint8_t)c[2].get<int>() });
            if (list.empty())
                list.push_back({ 255, 255, 255 });
        },
    };
}

namespace detail
{
template <class C>
Field CodeField(std::string C::* m, std::string name, std::string label, ParamType type)
{
    ParamDesc d{ std::move(name), std::move(label), type, 0, 0, 0, {} };
    return {
        d,
        [m, key = d.Name](const void* cfg, nlohmann::json& j) {
            j[key] = static_cast<const C*>(cfg)->*m;
        },
        [m, key = d.Name](void* cfg, const nlohmann::json& j) {
            static_cast<C*>(cfg)->*m = j.at(key).get<std::string>();
        },
    };
}
} // namespace detail

template <class C>
Field Glsl(std::string C::* m, std::string name, std::string label)
{
    return detail::CodeField(m, std::move(name), std::move(label), ParamType::Glsl);
}

template <class C>
Field Lua(std::string C::* m, std::string name, std::string label)
{
    return detail::CodeField(m, std::move(name), std::move(label), ParamType::Lua);
}

// ── Reflected effect base ─────────────────────────────────────────────────────
// Effects inherit ReflectedEffect<TheirConfig> and implement Fields()/EffectName().
// GetDescriptor/GetConfig/SetConfig are generated from the field table.
template <class Config>
class ReflectedEffect : public Effect
{
public:
    EffectDesc GetDescriptor() const override
    {
        EffectDesc d;
        d.Name = EffectName();
        for (const Field& f : Fields())
            d.Params.push_back(f.Desc);
        return d;
    }

    nlohmann::json GetConfig() const override
    {
        nlohmann::json j;
        for (const Field& f : Fields())
            f.Save(&Cfg, j);
        return j;
    }

    void SetConfig(const nlohmann::json& j) override
    {
        std::vector<std::string> changed;
        for (const Field& f : Fields())
        {
            if (j.contains(f.Desc.Name))
            {
                f.Load(&Cfg, j);
                changed.push_back(f.Desc.Name);
            }
        }
        if (!changed.empty())
            OnConfigChanged(changed);
    }

    // Direct typed access for bespoke UI files.
    Config&       ConfigRef()       { return Cfg; }
    const Config& ConfigRef() const { return Cfg; }

    // UI calls this after editing ConfigRef() fields directly so side-effects fire.
    void NotifyConfigChanged(const std::vector<std::string>& keys) { OnConfigChanged(keys); }

protected:
    virtual const std::vector<Field>& Fields() const = 0;
    virtual std::string EffectName() const = 0;
    virtual void OnConfigChanged(const std::vector<std::string>& /*changed*/) {}

    Config Cfg;
};
