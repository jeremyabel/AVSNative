#include "Bump.h"

#include "engine/JsonUtil.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_bump.sc.bin.h"

#include <algorithm>
#include <cmath>

// Built-in variable names that ScanVarDecls must not treat as user-declared.
static const std::vector<std::string> k_builtins = {
    "x", "y", "bi", "isBeat", "is_long_beat",
    "getspec", "getosc",
};

void Bump::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_bump_spv, sizeof(fs_bump_spv)));
    Program    = bgfx::createProgram(VS, FS, true);
    InputUnif  = bgfx::createUniform("s_input",      bgfx::UniformType::Sampler);
    TexelUnif  = bgfx::createUniform("u_texelSize",  bgfx::UniformType::Vec4);
    ParamsUnif = bgfx::createUniform("u_bumpParams", bgfx::UniformType::Vec4);
    FlagsUnif  = bgfx::createUniform("u_bumpFlags",  bgfx::UniformType::Vec4);

    m_curDepth = Depth;

    for (const auto& v : k_builtins) m_lua.SeedVar(v);
    m_lua.SetEnvNumber("bi", 1.0);

    Recompile();
    SeedUserVars();
    m_lua.RunBlock(m_initRef, "initCode");
    m_inited = true;
}

void Bump::SeedUserVars()
{
    const std::string all = InitCode + "\n" + FrameCode + "\n" + BeatCode;
    for (const auto& v : LuaRuntime::ScanVarDecls(all, k_builtins))
        m_lua.SeedVar(v);
}

void Bump::Recompile()
{
    m_lua.CompileBlock(InitCode,  "initCode",  m_initRef);
    m_lua.CompileBlock(FrameCode, "frameCode", m_frameRef);
    m_lua.CompileBlock(BeatCode,  "beatCode",  m_beatRef);
}

void Bump::RecompileCode()
{
    if (!m_inited) return;

    Recompile();
    SeedUserVars();
    m_lua.RunBlock(m_initRef, "initCode");
}

nlohmann::json Bump::Serialize() const
{
    return {
        { kDepth,          Depth          },
        { kOnBeat,         OnBeat         },
        { kOnBeatDuration, OnBeatDuration },
        { kOnBeatDepth,    OnBeatDepth    },
        { kBlendMode,      BlendMode      },
        { kShowLightPos,   ShowLightPos   },
        { kInvertDepth,    InvertDepth    },
        { kInitCode,       InitCode       },
        { kFrameCode,      FrameCode      },
        { kBeatCode,       BeatCode       },
    };
}

void Bump::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt   (j, kDepth,          Depth);
    JsonUtil::ReadBool  (j, kOnBeat,         OnBeat);
    JsonUtil::ReadInt   (j, kOnBeatDuration, OnBeatDuration);
    JsonUtil::ReadInt   (j, kOnBeatDepth,    OnBeatDepth);
    JsonUtil::ReadInt   (j, kBlendMode,      BlendMode);
    JsonUtil::ReadBool  (j, kShowLightPos,   ShowLightPos);
    JsonUtil::ReadBool  (j, kInvertDepth,    InvertDepth);
    JsonUtil::ReadString(j, kInitCode,       InitCode);
    JsonUtil::ReadString(j, kFrameCode,      FrameCode);
    JsonUtil::ReadString(j, kBeatCode,       BeatCode);

    RecompileCode();
}

void Bump::Render(const RenderContext& Context)
{
    const int w = Context.Width;
    const int h = Context.Height;

    // Update beat flags (EEL convention: -1 = true, 1 = false).
    m_lua.SetEnvNumber("isBeat",       Context.IsBeat() ? -1.0 : 1.0);
    m_lua.SetEnvNumber("is_long_beat", m_onBeatFadeout > 0 ? -1.0 : 1.0);

    m_lua.RunBlock(m_frameRef, "frameCode");
    if (Context.IsBeat())
        m_lua.RunBlock(m_beatRef, "beatCode");

    // Clamp bi to [0,1].
    const double bi = std::clamp(m_lua.GetEnvNumber("bi"), 0.0, 1.0);
    m_lua.SetEnvNumber("bi", bi);

    // On-beat depth snap (before bi multiplication, matching original).
    if (Context.IsBeat() && OnBeat)
    {
        m_curDepth      = OnBeatDepth;
        m_onBeatFadeout = OnBeatDuration;
    }
    else if (!m_onBeatFadeout)
    {
        m_curDepth = Depth;
    }

    m_curDepth = (int)((double)m_curDepth * bi);
    const int depthScaled = (m_curDepth * 256) / 100;

    // Light center: normalized [0,1] → pixel coords (truncate, clamp to [0, dim]).
    const int centerX = std::clamp((int)(m_lua.GetEnvNumber("x") * w), 0, w);
    const int centerY = std::clamp((int)(m_lua.GetEnvNumber("y") * h), 0, h);

    const float texelSz[4] = { 1.0f / (float)w, 1.0f / (float)h, (float)w, (float)h };
    const float params[4]  = { (float)centerX, (float)centerY, (float)depthScaled, (float)BlendMode };
    const float flags[4]   = { InvertDepth  ? 1.0f : 0.0f,
                                ShowLightPos ? 1.0f : 0.0f, 0.0f, 0.0f };

    bgfx::setUniform(TexelUnif,  texelSz);
    bgfx::setUniform(ParamsUnif, params);
    bgfx::setUniform(FlagsUnif,  flags);
    bgfx::setTexture(0, InputUnif, Context.InputTexture,
                     BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();

    // Advance on-beat fadeout: step cur_depth toward base depth each frame.
    if (m_onBeatFadeout > 0)
    {
        m_onBeatFadeout--;
        if (m_onBeatFadeout > 0 && OnBeatDuration > 0)
        {
            const int step = std::abs(Depth - OnBeatDepth) / OnBeatDuration;
            m_curDepth += step * (OnBeatDepth > Depth ? -1 : 1);
        }
    }
}

void Bump::Destroy()
{
    if (bgfx::isValid(FlagsUnif))  bgfx::destroy(FlagsUnif);
    if (bgfx::isValid(ParamsUnif)) bgfx::destroy(ParamsUnif);
    if (bgfx::isValid(TexelUnif))  bgfx::destroy(TexelUnif);
    if (bgfx::isValid(InputUnif))  bgfx::destroy(InputUnif);
    if (bgfx::isValid(Program))    bgfx::destroy(Program);

    FlagsUnif  = BGFX_INVALID_HANDLE;
    ParamsUnif = BGFX_INVALID_HANDLE;
    TexelUnif  = BGFX_INVALID_HANDLE;
    InputUnif  = BGFX_INVALID_HANDLE;
    Program    = BGFX_INVALID_HANDLE;
    m_inited   = false;
}
