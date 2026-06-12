#include "engine/KeyedImageList.h"
#include "engine/KeyInput.h"
#include "engine/JsonUtil.h"

#include <cctype>
#include <string>

void KeyedImageList::Serialize(nlohmann::json& j) const
{
    j["imageCount"]    = (int)Images.size();
    j["selectedImage"] = Selected;

    nlohmann::json keys = nlohmann::json::array();
    for (size_t i = 0; i < Images.size(); ++i)
    {
        j["image" + std::to_string(i)] = Images[i].Name;
        keys.push_back((uint32_t)Images[i].Keycode);
    }
    j["imageKeys"] = keys;
}

void KeyedImageList::Deserialize(const nlohmann::json& j)
{
    int count = 0;
    JsonUtil::ReadInt(j, "imageCount", count);
    if (count < 0)
        count = 0;
    Images.assign((size_t)count, KeyedImage{});

    // Keycodes (parallel to entries). Stored/read as unsigned to cover SDL keycodes
    // that carry the scancode mask bit (function keys, arrows, etc.).
    if (auto it = j.find("imageKeys"); it != j.end() && it->is_array())
    {
        for (size_t i = 0; i < Images.size() && i < it->size(); ++i)
            if ((*it)[i].is_number())
                Images[i].Keycode = (*it)[i].get<uint32_t>();
    }

    // Names (placeholder; the real basename + bytes arrive via ApplyAsset on bundle
    // load). Kept so plain-JSON round-trips at least retain the saved string.
    for (size_t i = 0; i < Images.size(); ++i)
    {
        const std::string key = "image" + std::to_string(i);
        if (auto it = j.find(key); it != j.end() && it->is_string())
            Images[i].Name = it->get<std::string>();
    }

    Selected = 0;
    JsonUtil::ReadInt(j, "selectedImage", Selected);
    ClampSelected();
}

void KeyedImageList::CollectAssets(std::vector<PresetAsset>& out) const
{
    for (size_t i = 0; i < Images.size(); ++i)
        if (!Images[i].Raw.empty())
            out.push_back({ "image" + std::to_string(i), Images[i].Name, Images[i].Raw });
}

bool KeyedImageList::ApplyAsset(const std::string& key, std::string name,
                                std::vector<uint8_t> bytes)
{
    // Match "image" followed by one-or-more digits ("imageData" has non-digits → no).
    if (key.rfind("image", 0) != 0)
        return false;
    const std::string idxStr = key.substr(5);
    if (idxStr.empty())
        return false;
    for (char c : idxStr)
        if (!std::isdigit((unsigned char)c))
            return false;

    const size_t idx = (size_t)std::stoul(idxStr);
    if (idx >= Images.size())
        Images.resize(idx + 1);
    Images[idx].Raw  = std::move(bytes);
    Images[idx].Name = std::move(name);
    return true;
}

bool KeyedImageList::UpdateSelection()
{
    for (size_t i = 0; i < Images.size(); ++i)
    {
        if (Images[i].Keycode != 0 && avs::KeyInput::WasKeyPressed(Images[i].Keycode))
        {
            if ((int)i != Selected)
            {
                Selected = (int)i;
                return true;
            }
            return false;  // already showing this image — pressing again is a no-op
        }
    }
    return false;
}

void KeyedImageList::ClampSelected()
{
    if (Images.empty())
        Selected = 0;
    else if (Selected < 0)
        Selected = 0;
    else if (Selected >= (int)Images.size())
        Selected = (int)Images.size() - 1;
}
