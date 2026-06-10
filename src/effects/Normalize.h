#pragma once

#include "engine/Reflect.h"
#include "engine/ShaderCompiler.h"

#include <vector>

struct NormalizeConfig {};

class Normalize : public ReflectedEffect<NormalizeConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    // Multi-pass: init + reduce chain + apply. 20 covers any resolution up to ~4K.
    uint8_t ExpectedViewCount() const override { return 20; }

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f;
        return f;
    }
    std::string EffectName() const override { return "Normalize"; }

private:
    struct Level {
        bgfx::TextureHandle     Tex = BGFX_INVALID_HANDLE;
        bgfx::FrameBufferHandle Fbo = BGFX_INVALID_HANDLE;
        int W = 0, H = 0;
    };

    void EnsureChain(int W, int H);
    void DestroyChain();
    void CompileProgram(const char* FragGlsl, const BgfxUniformDesc* Uniforms,
                        int UniformCount, uint16_t UboSize,
                        bgfx::ProgramHandle& Out) const;

    bgfx::ProgramHandle m_initProg   = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_reduceProg = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_applyProg  = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle m_srcSizeUnif = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputUnif   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_minMaxUnif  = BGFX_INVALID_HANDLE;

    std::vector<Level> m_chain;
    int m_chainW = -1, m_chainH = -1;
};
