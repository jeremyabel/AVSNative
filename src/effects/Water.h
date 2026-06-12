#pragma once

#include "engine/Effect.h"

class Water : public Effect
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Water"; }
    nlohmann::json Serialize() const override { return nlohmann::json::object(); }
    void Deserialize(const nlohmann::json& /*j*/) override {}

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
