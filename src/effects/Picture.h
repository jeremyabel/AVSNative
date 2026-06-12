#pragma once

#include "engine/Effect.h"

#include <bgfx/bgfx.h>
#include <cstdint>
#include <string>
#include <vector>

class Picture : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int  BlendMode      = 2;     // 0=Replace, 1=Additive, 2=50/50
    bool OnBeatAdditive = false;
    int  OnBeatDuration = 6;     // 0..32
    int  Fit            = 0;     // 0=Stretch, 1=FitWidth, 2=FitHeight
    std::string ImageData = "";  // bundle asset ref/name; raw bytes arrive via ApplyAsset

    static constexpr const char* kBlendMode      = "blendMode";
    static constexpr const char* kOnBeatAdditive = "onBeatAdditive";
    static constexpr const char* kOnBeatDuration = "onBeatDuration";
    static constexpr const char* kFit            = "fit";
    static constexpr const char* kImageData      = "imageData";

    void Init() override;
    void Render(const RenderContext& Ctx) override;
    void Destroy() override;

    std::string Name() const override { return "Picture"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    std::vector<PresetAsset> CollectAssets() const override;
    void ApplyAsset(const std::string& key, const std::string& name,
                    std::vector<uint8_t> bytes) override;

    int GetImageW() const { return m_imgW; }
    int GetImageH() const { return m_imgH; }

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
