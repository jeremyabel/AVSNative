#pragma once

#include "engine/Effect.h"

#include <vector>

class Normalize : public Effect
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Normalize"; }
    nlohmann::json Serialize() const override { return nlohmann::json::object(); }
    void Deserialize(const nlohmann::json& /*j*/) override {}

    // Multi-pass: init + reduce chain + apply. 20 covers any resolution up to ~4K.
    uint8_t ExpectedViewCount() const override { return 20; }

private:

    struct Level {
        bgfx::TextureHandle Tex = BGFX_INVALID_HANDLE;
        bgfx::FrameBufferHandle Fbo = BGFX_INVALID_HANDLE;
        int W = 0, H = 0;
    };

    void EnsureChain(int W, int H);
    void DestroyChain();

    bgfx::ProgramHandle m_initProg = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_reduceProg = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_applyProg = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle m_srcSizeUnif = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputUnif = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_minMaxUnif = BGFX_INVALID_HANDLE;

    std::vector<Level> m_chain;
    int m_chainW = -1;
    int m_chainH = -1;
};
