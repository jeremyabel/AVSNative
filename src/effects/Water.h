#pragma once

#include "engine/Reflect.h"

struct WaterConfig
{
};

class Water : public ReflectedEffect<WaterConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {};
        return f;
    }
    std::string EffectName() const override { return "Water"; }

private:
    void EnsurePrev(uint16_t W, uint16_t H);

    bgfx::ProgramHandle WaterProgram  = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle BlitProgram   = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;  // s_texColor slot 0
    bgfx::UniformHandle PrevUniform   = BGFX_INVALID_HANDLE;  // s_prevTex  slot 1
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;

    bgfx::TextureHandle     PrevTexture = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle PrevFBO     = BGFX_INVALID_HANDLE;
    uint16_t PrevW = 0;
    uint16_t PrevH = 0;
};
