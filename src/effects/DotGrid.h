#pragma once

#include "engine/Reflect.h"

struct DotGridConfig
{
    std::vector<std::array<uint8_t, 3>> Colors = { { { 255, 255, 255 } } };
    int Spacing   = 8;
    int SpeedX    = 128;   // fixed-point 8.8: 128 = 0.5 px/frame
    int SpeedY    = 128;
    int BlendMode = 3;
};

class DotGrid : public ReflectedEffect<DotGridConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    // DotGrid keeps a legacy "color" alias (= Colors[0]) alongside the "colors"
    // array for backward-compatible presets, so it augments the generic JSON.
    nlohmann::json GetConfig() const override
    {
        nlohmann::json j = ReflectedEffect::GetConfig();
        if (!Cfg.Colors.empty())
            j["color"] = { (int)Cfg.Colors[0][0], (int)Cfg.Colors[0][1], (int)Cfg.Colors[0][2] };
        return j;
    }

    void SetConfig(const nlohmann::json& j) override
    {
        ReflectedEffect::SetConfig(j);
        if (j.contains("color") && j["color"].is_array() && j["color"].size() == 3 && !Cfg.Colors.empty())
            Cfg.Colors[0] = { (uint8_t)j["color"][0].get<int>(),
                              (uint8_t)j["color"][1].get<int>(),
                              (uint8_t)j["color"][2].get<int>() };
    }

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Colors(&DotGridConfig::Colors, "colors", "Colors"),
            RangeI(&DotGridConfig::Spacing, "spacing", "Spacing", 2, 64),
            RangeI(&DotGridConfig::SpeedX, "speedX", "Speed X", -512, 544),
            RangeI(&DotGridConfig::SpeedY, "speedY", "Speed Y", -512, 544),
            SelectI(&DotGridConfig::BlendMode, "blendMode", "Blend Mode",
                    { "Replace", "Additive", "50/50", "Default" }),
        };
        return f;
    }
    std::string EffectName() const override { return "Dot Grid"; }

    void OnConfigChanged(const std::vector<std::string>& changed) override
    {
        for (const std::string& k : changed)
            if (k == "colors")
                ColorPos = 0;
    }

private:
    // Runtime state
    int32_t Xp       = 0;   // fixed-point 8.8 scroll accumulator
    int32_t Yp       = 0;
    int32_t ColorPos = 0;   // 0 .. Colors.size()*64 - 1

    bgfx::ProgramHandle Program      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle GridUniform  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle SizeUniform  = BGFX_INVALID_HANDLE;
};
