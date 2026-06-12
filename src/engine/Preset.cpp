#include "Preset.h"

#include "Effect.h"
#include "EffectChain.h"
#include "Engine.h"
#include "Registry.h"
#include "ZipArchive.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <vector>

// ── Helpers ───────────────────────────────────────────────────────────────────

// Basename of a forward-slash path: "assets/cat.gif" -> "cat.gif".
static std::string Basename(const std::string& p)
{
    const size_t s = p.find_last_of('/');
    return (s == std::string::npos) ? p : p.substr(s + 1);
}

// Pick a unique bundle entry name, inserting "_N" before the extension on collision.
static std::string UniqueAssetName(const std::string& base, std::set<std::string>& used)
{
    if (used.insert(base).second)
        return base;

    const size_t slash = base.find_last_of('/');
    const size_t dot   = base.find_last_of('.');
    std::string stem, ext;
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
    {
        stem = base.substr(0, dot);
        ext  = base.substr(dot);
    }
    else
    {
        stem = base;
    }
    for (int n = 1; ; ++n)
    {
        std::string cand = stem + "_" + std::to_string(n) + ext;
        if (used.insert(cand).second)
            return cand;
    }
}

// ── Load ──────────────────────────────────────────────────────────────────────

// Recursively loads an effects JSON array into Chain, creating and configuring
// each effect via Reg. Handles nested EffectLists by loading their inner chain
// the same way. When 'assets' is non-null (bundle load), any string config value
// referencing "assets/..." is resolved to raw bytes and delivered via ApplyAsset.
static void LoadChain(EffectChain& Chain,
                      const nlohmann::json& EffectsArray,
                      Registry& Reg,
                      const std::vector<ZipArchive::Entry>* assets)
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
            Effect->Deserialize(Cfg);

            // Resolve bundled asset references (after Deserialize) to raw bytes.
            if (assets)
            {
                for (auto it = Cfg.begin(); it != Cfg.end(); ++it)
                {
                    if (!it.value().is_string())
                        continue;
                    const std::string v = it.value().get<std::string>();
                    if (v.rfind("assets/", 0) != 0)
                        continue;
                    if (const std::vector<uint8_t>* bytes = ZipArchive::Find(*assets, v))
                        Effect->ApplyAsset(it.key(), Basename(v), *bytes);
                }
            }

            // If this effect owns an inner chain (e.g. EffectList), populate it
            // recursively using the same registry — Deserialize alone can't do this
            // because it has no access to the Registry.
            EffectChain* Inner = Effect->GetInnerChain();
            if (Inner && Cfg.contains("effects") && Cfg["effects"].is_array())
                LoadChain(*Inner, Cfg["effects"], Reg, assets);
        }

        Chain.Add(std::move(Effect));

        if (!Enabled)
            Chain.GetEntry(Chain.Count() - 1).Enabled = false;
    }
}

bool Preset::Load(const char* Path, Engine& Engine)
{
    // Detect bundle (.avsz zip) vs plain JSON by the leading magic bytes.
    bool isZip = false;
    {
        std::ifstream probe(Path, std::ios::binary);
        if (!probe.is_open())
        {
            fprintf(stderr, "Preset: cannot open '%s'\n", Path);
            return false;
        }
        unsigned char magic[4] = { 0, 0, 0, 0 };
        probe.read(reinterpret_cast<char*>(magic), 4);
        isZip = (magic[0] == 'P' && magic[1] == 'K' && magic[2] == 0x03 && magic[3] == 0x04);
    }

    nlohmann::json Json;
    std::vector<ZipArchive::Entry>        entries;
    const std::vector<ZipArchive::Entry>* assets = nullptr;

    if (isZip)
    {
        if (!ZipArchive::Read(Path, entries))
        {
            fprintf(stderr, "Preset: cannot read bundle '%s'\n", Path);
            return false;
        }
        const std::vector<uint8_t>* pj = ZipArchive::Find(entries, "preset.json");
        if (!pj)
        {
            fprintf(stderr, "Preset: bundle '%s' has no preset.json\n", Path);
            return false;
        }
        try
        {
            Json = nlohmann::json::parse(pj->begin(), pj->end());
        }
        catch (const std::exception& Ex)
        {
            fprintf(stderr, "Preset: JSON parse error in bundle '%s': %s\n", Path, Ex.what());
            return false;
        }
        assets = &entries;
    }
    else
    {
        std::ifstream File(Path);
        if (!File.is_open())
        {
            fprintf(stderr, "Preset: cannot open '%s'\n", Path);
            return false;
        }
        try
        {
            Json = nlohmann::json::parse(File);
        }
        catch (const std::exception& Ex)
        {
            fprintf(stderr, "Preset: JSON parse error in '%s': %s\n", Path, Ex.what());
            return false;
        }
    }

    if (!Json.contains("effects"))
        return true;

    LoadChain(Engine.GetChain(), Json["effects"], Engine.GetRegistry(), assets);
    return true;
}

// ── Save ──────────────────────────────────────────────────────────────────────

// Recursively serialise a chain to JSON. Each effect's binary assets are pulled
// out (CollectAssets), assigned a unique "assets/<name>" bundle entry, and the
// effect's config key is rewritten to that path. Nested EffectList chains recurse,
// rebuilding config["effects"] so nested asset refs are rewritten too.
static nlohmann::json SerialiseChain(EffectChain& Chain,
                                     std::vector<ZipArchive::Entry>& assets,
                                     std::set<std::string>& usedNames)
{
    nlohmann::json effects = nlohmann::json::array();
    for (int32_t i = 0; i < Chain.Count(); ++i)
    {
        EffectEntry& Entry = Chain.GetEntry(i);

        nlohmann::json item;
        item["type"]    = Entry.Effect->Name();
        item["enabled"] = Entry.Enabled;

        nlohmann::json cfg = Entry.Effect->Serialize();

        for (PresetAsset& a : Entry.Effect->CollectAssets())
        {
            const std::string nm = a.Name.empty() ? (a.Key + ".bin") : a.Name;
            const std::string entryName = UniqueAssetName("assets/" + nm, usedNames);
            assets.push_back({ entryName, std::move(a.Bytes) });
            cfg[a.Key] = entryName;
        }

        if (EffectChain* Inner = Entry.Effect->GetInnerChain())
            cfg["effects"] = SerialiseChain(*Inner, assets, usedNames);

        item["config"] = std::move(cfg);
        effects.push_back(std::move(item));
    }
    return effects;
}

bool Preset::Save(const char* Path, Engine& Engine)
{
    std::vector<ZipArchive::Entry> assets;
    std::set<std::string>          usedNames;

    nlohmann::json Json;
    Json["effects"] = SerialiseChain(Engine.GetChain(), assets, usedNames);

    const std::string dumped = Json.dump(2);

    std::vector<ZipArchive::Entry> entries;
    entries.reserve(assets.size() + 1);
    entries.push_back({ "preset.json",
                        std::vector<uint8_t>(dumped.begin(), dumped.end()) });
    for (auto& a : assets)
        entries.push_back(std::move(a));

    if (!ZipArchive::Write(Path, entries))
    {
        fprintf(stderr, "Preset: cannot write '%s'\n", Path);
        return false;
    }
    return true;
}
