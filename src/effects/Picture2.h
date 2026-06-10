#pragma once

#include "engine/Reflect.h"

#include <bgfx/bgfx.h>
#include <string>

struct Picture2Config
{
    int  BlendMode         = 0;     // 0-10 (see blend modes in fs_picture2.sc)
    int  OnBeatBlendMode   = 0;
    bool Bilinear          = true;
    bool OnBeatBilinear    = true;
    int  AdjustBlend       = 128;   // 0-255, used when BlendMode == 7 (Adjustable)
    int  OnBeatAdjustBlend = 128;
    std::string ImageData  = "";    // base64 data URL, managed outside reflection
};

class Picture2 : public ReflectedEffect<Picture2Config>
{
public:
    void Init() override;
    void Render(const RenderContext& Ctx) override;
    void Destroy() override;

    nlohmann::json GetConfig() const override;
    void           SetConfig(const nlohmann::json& cfg) override;

    int GetImageW() const { return m_imgW; }
    int GetImageH() const { return m_imgH; }

protected:
    const std::vector<Field>& Fields() const override;
    std::string EffectName() const override { return "Picture II"; }
    void OnConfigChanged(const std::vector<std::string>& changed) override;

private:
    void LoadImage(const std::string& dataUrl);

    bgfx::ProgramHandle m_prog       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputUnif  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_imageUnif  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUnif = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_imageTex   = BGFX_INVALID_HANDLE;

    int  m_imgW   = 0;
    int  m_imgH   = 0;
    bool m_inited = false;
};
