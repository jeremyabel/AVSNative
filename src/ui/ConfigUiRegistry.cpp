#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "engine/Effect.h"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <string>

void ConfigUiRegistry::Register(const std::string& name, EffectUiDraw draw)
{
    m_draws[name] = std::move(draw);
}

const EffectUiDraw* ConfigUiRegistry::Find(const std::string& name) const
{
    auto it = m_draws.find(name);
    return it == m_draws.end() ? nullptr : &it->second;
}

// ── Descriptor-driven auto-generated UI ───────────────────────────────────────
// Generic, JSON-based: works for any reflected effect without a bespoke layout.
void DrawDefault(Effect* effect)
{
    const EffectDesc desc = effect->GetDescriptor();
    nlohmann::json   cfg  = effect->GetConfig();

    for (const ParamDesc& p : desc.Params)
    {
        ImGui::PushID(p.Name.c_str());

        switch (p.Type)
        {
        case ParamType::Range:
        {
            float val = cfg.value(p.Name, 0.0f);
            ImGui::TextUnformatted(p.Label.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderFloat("##v", &val, p.Min, p.Max))
                effect->SetConfig({ { p.Name, val } });
            break;
        }
        case ParamType::Number:
        {
            float val = cfg.value(p.Name, 0.0f);
            ImGui::TextUnformatted(p.Label.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputFloat("##v", &val, p.Step, p.Step * 10.0f))
                effect->SetConfig({ { p.Name, val } });
            break;
        }
        case ParamType::Bool:
        {
            bool val = cfg.value(p.Name, false);
            if (ImGui::Checkbox(p.Label.c_str(), &val))
                effect->SetConfig({ { p.Name, val } });
            break;
        }
        case ParamType::Color:
        {
            float col[3] = { 0.0f, 0.0f, 0.0f };
            if (cfg.contains(p.Name) && cfg[p.Name].is_array() && cfg[p.Name].size() == 3)
            {
                col[0] = cfg[p.Name][0].get<int>() / 255.0f;
                col[1] = cfg[p.Name][1].get<int>() / 255.0f;
                col[2] = cfg[p.Name][2].get<int>() / 255.0f;
            }
            ImGui::TextUnformatted(p.Label.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::ColorEdit3("##v", col))
            {
                effect->SetConfig({ { p.Name,
                                      { (int)(col[0] * 255.0f + 0.5f),
                                        (int)(col[1] * 255.0f + 0.5f),
                                        (int)(col[2] * 255.0f + 0.5f) } } });
            }
            break;
        }
        case ParamType::Select:
        {
            const auto& opts          = p.Options;
            bool        storedAsString = cfg.contains(p.Name) && cfg[p.Name].is_string();
            int         cur            = 0;
            if (cfg.contains(p.Name))
            {
                if (cfg[p.Name].is_number_integer())
                {
                    cur = cfg[p.Name].get<int>();
                }
                else if (cfg[p.Name].is_string())
                {
                    const std::string sv = cfg[p.Name].get<std::string>();
                    for (int i = 0; i < (int)opts.size(); ++i)
                        if (opts[i] == sv) { cur = i; break; }
                }
            }
            const char* preview = (cur >= 0 && cur < (int)opts.size()) ? opts[cur].c_str()
                                                                       : "(invalid)";
            ImGui::TextUnformatted(p.Label.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##v", preview))
            {
                for (int i = 0; i < (int)opts.size(); ++i)
                {
                    bool picked = (i == cur);
                    if (ImGui::Selectable(opts[i].c_str(), picked))
                    {
                        if (storedAsString)
                            effect->SetConfig({ { p.Name, opts[i] } });
                        else
                            effect->SetConfig({ { p.Name, i } });
                    }
                    if (picked) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            break;
        }
        case ParamType::Glsl:
        case ParamType::Lua:
        {
            std::string text = cfg.value(p.Name, std::string{});
            ImGui::TextUnformatted(p.Label.c_str());
            if (ConfigUi::CodeEditor(p.Name.c_str(), text,
                                     p.Type == ParamType::Glsl ? ConfigUi::Lang::Glsl
                                                               : ConfigUi::Lang::Lua))
            {
                effect->SetConfig({ { p.Name, text } });
            }
            const std::string err = effect->GetScriptError(p.Name);
            if (!err.empty())
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", err.c_str());
            break;
        }
        case ParamType::Colors:
        {
            ImGui::TextUnformatted(p.Label.c_str());
            if (cfg.contains(p.Name) && cfg[p.Name].is_array())
            {
                nlohmann::json colorList   = cfg[p.Name];
                bool           listChanged = false;

                for (int i = 0; i < (int)colorList.size(); ++i)
                {
                    ImGui::PushID(i);
                    float col[3] = { colorList[i][0].get<int>() / 255.0f,
                                     colorList[i][1].get<int>() / 255.0f,
                                     colorList[i][2].get<int>() / 255.0f };
                    char label[16];
                    snprintf(label, sizeof(label), "##c%d", i);
                    if (ImGui::ColorEdit3(label, col,
                                          ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoInputs))
                    {
                        colorList[i] = { (int)(col[0] * 255.0f + 0.5f),
                                         (int)(col[1] * 255.0f + 0.5f),
                                         (int)(col[2] * 255.0f + 0.5f) };
                        listChanged = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x") && colorList.size() > 1)
                    {
                        colorList.erase(i);
                        listChanged = true;
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }
                if (ImGui::SmallButton("+"))
                {
                    colorList.push_back({ 255, 255, 255 });
                    listChanged = true;
                }
                if (listChanged)
                    effect->SetConfig({ { p.Name, colorList } });
            }
            break;
        }
        }

        ImGui::PopID();
        ImGui::Spacing();
    }
}

// ── Central registration ──────────────────────────────────────────────────────
// One forward declaration + call per effect UI file (see src/ui/effects/).
#define EFFECT_UI(Name) void Register##Name##UI(ConfigUiRegistry&);
EFFECT_UI(AddBorders)
EFFECT_UI(BufferSave)
EFFECT_UI(ChannelShift)
EFFECT_UI(Clear)
EFFECT_UI(Grain)
EFFECT_UI(Invert)
EFFECT_UI(Scatter)
EFFECT_UI(UniqueTone)
EFFECT_UI(EffectList)
EFFECT_UI(FadeOut)
EFFECT_UI(MovingParticle)
EFFECT_UI(Movement)
EFFECT_UI(Starfield)
EFFECT_UI(Simple)
EFFECT_UI(Mosaic)
EFFECT_UI(FastBrightness)
EFFECT_UI(Mirror)
EFFECT_UI(Blur)
EFFECT_UI(ColorReduction)
EFFECT_UI(ColorClip)
EFFECT_UI(BlitEffect)
EFFECT_UI(RotoBlitter)
EFFECT_UI(ColorFade)
EFFECT_UI(SetRenderMode)
EFFECT_UI(SuperScope)
EFFECT_UI(Interferences)
EFFECT_UI(Interleave)
EFFECT_UI(MultiFilter)
EFFECT_UI(Multiplier)
EFFECT_UI(OnBeatClear)
EFFECT_UI(Bump)
EFFECT_UI(Water)
EFFECT_UI(WaterBump)
EFFECT_UI(DotGrid)
EFFECT_UI(BassSpin)
EFFECT_UI(Normalize)
EFFECT_UI(ColorModifier)
EFFECT_UI(RotatingStars)
EFFECT_UI(OscilloscopeStar)
EFFECT_UI(Ring)
EFFECT_UI(Picture)
EFFECT_UI(Picture2)
EFFECT_UI(Convolution)
EFFECT_UI(ColorMap)
EFFECT_UI(CustomBpm)
EFFECT_UI(DynamicDistanceModifier)
EFFECT_UI(DynamicShift)
EFFECT_UI(Texer)
EFFECT_UI(Texer2)
EFFECT_UI(MultiDelay)
EFFECT_UI(Triangle)
EFFECT_UI(VideoDelay)
EFFECT_UI(DynamicMovement)
EFFECT_UI(DotFountain)
EFFECT_UI(DotPlane)
EFFECT_UI(Timescope)
EFFECT_UI(Brightness)
EFFECT_UI(Text)
#undef EFFECT_UI

void RegisterAllEffectUis(ConfigUiRegistry& reg)
{
#define EFFECT_UI(Name) Register##Name##UI(reg);
    EFFECT_UI(AddBorders)
    EFFECT_UI(BufferSave)
    EFFECT_UI(ChannelShift)
    EFFECT_UI(Clear)
    EFFECT_UI(Grain)
    EFFECT_UI(Invert)
    EFFECT_UI(Scatter)
    EFFECT_UI(UniqueTone)
    EFFECT_UI(EffectList)
    EFFECT_UI(FadeOut)
    EFFECT_UI(MovingParticle)
    EFFECT_UI(Movement)
    EFFECT_UI(Starfield)
    EFFECT_UI(Simple)
    EFFECT_UI(Mosaic)
    EFFECT_UI(FastBrightness)
    EFFECT_UI(Mirror)
    EFFECT_UI(Blur)
    EFFECT_UI(ColorReduction)
    EFFECT_UI(ColorClip)
    EFFECT_UI(BlitEffect)
    EFFECT_UI(RotoBlitter)
    EFFECT_UI(ColorFade)
    EFFECT_UI(SetRenderMode)
    EFFECT_UI(SuperScope)
    EFFECT_UI(Interferences)
    EFFECT_UI(Interleave)
    EFFECT_UI(MultiFilter)
    EFFECT_UI(Multiplier)
    EFFECT_UI(OnBeatClear)
    EFFECT_UI(Bump)
    EFFECT_UI(Water)
    EFFECT_UI(WaterBump)
    EFFECT_UI(DotGrid)
    EFFECT_UI(BassSpin)
    EFFECT_UI(Normalize)
    EFFECT_UI(ColorModifier)
    EFFECT_UI(RotatingStars)
    EFFECT_UI(OscilloscopeStar)
    EFFECT_UI(Ring)
    EFFECT_UI(Picture)
    EFFECT_UI(Picture2)
    EFFECT_UI(Convolution)
    EFFECT_UI(ColorMap)
    EFFECT_UI(CustomBpm)
    EFFECT_UI(DynamicDistanceModifier)
    EFFECT_UI(DynamicShift)
    EFFECT_UI(Texer)
    EFFECT_UI(Texer2)
    EFFECT_UI(MultiDelay)
    EFFECT_UI(Triangle)
    EFFECT_UI(VideoDelay)
    EFFECT_UI(DynamicMovement)
    EFFECT_UI(DotFountain)
    EFFECT_UI(DotPlane)
    EFFECT_UI(Timescope)
    EFFECT_UI(Brightness)
    EFFECT_UI(Text)
#undef EFFECT_UI
}
