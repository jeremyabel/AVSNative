#pragma once

#include "engine/Reflect.h"

#include <bgfx/bgfx.h>
#include <string>

struct PictureConfig
{
    int  BlendMode      = 2;     // 0=Replace, 1=Additive, 2=50/50
    bool OnBeatAdditive = false;
    int  OnBeatDuration = 6;     // 0..32
    int  Fit            = 0;     // 0=Stretch, 1=FitWidth, 2=FitHeight
    std::string ImageData = "";  // base64 data URL, managed outside reflection
};

class Picture : public ReflectedEffect<PictureConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Ctx) override;
    void Destroy() override;

    // Augments reflected JSON with the large imageData string field.
    nlohmann::json GetConfig() const override;
    void           SetConfig(const nlohmann::json& cfg) override;

    int GetImageW() const { return m_imgW; }
    int GetImageH() const { return m_imgH; }

protected:
    const std::vector<Field>& Fields() const override;
    std::string EffectName() const override { return "Picture"; }
    void OnConfigChanged(const std::vector<std::string>& changed) override;

private:
    void LoadImage(const std::string& dataUrl);

    bgfx::ProgramHandle m_prog       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputUnif  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_imageUnif  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUnif = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_imageTex   = BGFX_INVALID_HANDLE;

    int  m_imgW     = 0;
    int  m_imgH     = 0;
    int  m_cooldown = 0;
    bool m_inited   = false;
};
