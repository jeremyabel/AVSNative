#include "Preset.h"

#include "Effect.h"
#include "EffectChain.h"
#include "Engine.h"
#include "Registry.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>

// Recursively loads an effects JSON array into Chain, creating and configuring
// each effect via Reg. Handles nested EffectLists by loading their inner chain
// the same way.
static void LoadChain(EffectChain& Chain,
                      const nlohmann::json& EffectsArray,
                      Registry& Reg)
{
    for (const auto& Item : EffectsArray)
    {
        if (!Item.contains("type"))
            continue;

        std::string Type = Item["type"].get<std::string>();
        bool Enabled = Item.value("enabled", true);

        auto Effect = Reg.Create(Type);
        if (!Effect)
        {
            fprintf(stderr, "Preset: unknown effect type '%s'\n", Type.c_str());
            continue;
        }

        Effect->Init();

        if (Item.contains("config"))
        {
            const auto& Cfg = Item["config"];
            Effect->SetConfig(Cfg);

            // If this effect owns an inner chain (e.g. EffectList), populate it
            // recursively using the same registry — SetConfig alone can't do this
            // because it has no access to the Registry.
            EffectChain* Inner = Effect->GetInnerChain();
            if (Inner && Cfg.contains("effects") && Cfg["effects"].is_array())
                LoadChain(*Inner, Cfg["effects"], Reg);
        }

        Chain.Add(std::move(Effect));

        if (!Enabled)
            Chain.GetEntry(Chain.Count() - 1).Enabled = false;
    }
}

bool Preset::Load(const char* Path, Engine& Engine)
{
    std::ifstream File(Path);
    if (!File.is_open())
    {
        fprintf(stderr, "Preset: cannot open '%s'\n", Path);
        return false;
    }

    nlohmann::json Json;
    try
    {
        Json = nlohmann::json::parse(File);
    }
    catch (const std::exception& Ex)
    {
        fprintf(stderr, "Preset: JSON parse error in '%s': %s\n", Path, Ex.what());
        return false;
    }

    if (!Json.contains("effects"))
        return true;

    LoadChain(Engine.GetChain(), Json["effects"], Engine.GetRegistry());
    return true;
}

static nlohmann::json SerialiseChain(EffectChain& Chain)
{
    nlohmann::json effects = nlohmann::json::array();
    for (int32_t i = 0; i < Chain.Count(); ++i)
    {
        EffectEntry& Entry = Chain.GetEntry(i);
        nlohmann::json item;
        item["type"]    = Entry.Effect->GetDescriptor().Name;
        item["enabled"] = Entry.Enabled;
        item["config"]  = Entry.Effect->GetConfig();
        effects.push_back(item);
    }
    return effects;
}

bool Preset::Save(const char* Path, Engine& Engine)
{
    nlohmann::json Json;
    Json["effects"] = SerialiseChain(Engine.GetChain());

    std::ofstream File(Path);
    if (!File.is_open())
    {
        fprintf(stderr, "Preset: cannot write '%s'\n", Path);
        return false;
    }

    File << Json.dump(2);
    return File.good();
}
