#pragma once

#include "engine/Reflect.h"

#include <bgfx/bgfx.h>
#include <cstdint>
#include <string>
#include <vector>

struct PictureConfig
{
    int  BlendMode      = 2;     // 0=Replace, 1=Additive, 2=50/50
    bool OnBeatAdditive = false;
    int  OnBeatDuration = 6;     // 0..32
    int  Fit            = 0;     // 0=Stretch, 1=FitWidth, 2=FitHeight
    std::string ImageData = "";  // bundle asset ref/name; raw bytes arrive via ApplyAsset
};

class Picture : public ReflectedEffect<PictureConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Ctx) override;
    void Destroy() override;

    // Augments reflected JSON with the imageData reference field.
    nlohmann::json GetConfig() const override;
    void           SetConfig(const nlohmann::json& cfg) override;

    std::vector<PresetAsset> CollectAssets() const override;
    void ApplyAsset(const std::string& key, const std::string& name,
                    std::vector<uint8_t> bytes) override;

    int GetImageW() const { return m_imgW; }
    int GetImageH() const { return m_imgH; }

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> kFields = {
            SelectI(&PictureConfig::BlendMode, "blendMode", "Blend Mode",
                    { "Replace", "Additive", "50/50" }),
            ::Bool(&PictureConfig::OnBeatAdditive, "onBeatAdditive", "On-Beat Additive"),
            RangeI(&PictureConfig::OnBeatDuration, "onBeatDuration", "On-Beat Duration", 0, 32),
            SelectI(&PictureConfig::Fit, "fit", "Image Fit",
                    { "Stretch", "Fit Width", "Fit Height" }),
        };
        return kFields;
    }
    std::string EffectName() const override { return "Picture"; }
    void OnConfigChanged(const std::vector<std::string>& changed) override;

private:
    void BuildFromRaw(const std::vector<uint8_t>& raw);

    bgfx::ProgramHandle m_prog       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputUnif  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_imageUnif  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUnif = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_imageTex   = BGFX_INVALID_HANDLE;

    std::vector<uint8_t> m_raw;   // original image bytes (for re-bundling)
    std::string          m_name;  // original filename

    int  m_imgW     = 0;
    int  m_imgH     = 0;
    int  m_cooldown = 0;
    bool m_inited   = false;
};
