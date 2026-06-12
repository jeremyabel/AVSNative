#include "OnBeatClear.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_onbeatclear.sc.bin.h"

void OnBeatClear::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_onbeatclear_spv, sizeof(fs_onbeatclear_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ColorUniform = bgfx::createUniform("u_obcColor", bgfx::UniformType::Vec4);
}

void OnBeatClear::Render(const RenderContext& Context)
{
    if (Context.IsBeat())
    {
        // nf=0 disables the effect entirely (matches `if (nf && ++cf >= nf)`)
        if (Nf && ++Cf >= Nf)
        {
            Cf = Df = 0;

            const float ClearColor[4] = { Color[0] / 255.f, Color[1] / 255.f, Color[2] / 255.f, Blend ? 1.f : 0.f };

            bgfx::setUniform(ColorUniform, ClearColor);
            bgfx::setTexture(0, TexUniform, Context.InputTexture);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
            bgfx::setVertexBuffer(0, Context.QuadVB);
            bgfx::submit(Context.ViewId, Program);

            Context.FboManager->Swap();
        }
    }
    else
    {
        if (++Df >= Nf)
        {
            Df = 0;
        }
    }
}

void OnBeatClear::Destroy()
{
    if (bgfx::isValid(ColorUniform))
        bgfx::destroy(ColorUniform);
    
    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    ColorUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

nlohmann::json OnBeatClear::Serialize() const
{
    return {
        { kColor, JsonUtil::ColorToJson(Color) },
        { kBlend, Blend },
        { kNf,    Nf    },
    };
}

void OnBeatClear::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadColor(j, kColor, Color);
    JsonUtil::ReadBool (j, kBlend, Blend);
    JsonUtil::ReadInt  (j, kNf,    Nf);
}
