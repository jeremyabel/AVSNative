#include "Bump.h"

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

    m_curDepth = Cfg.Depth;

    for (const auto& v : k_builtins) m_lua.SeedVar(v);
    m_lua.SetEnvNumber("bi", 1.0);

    Recompile();
    SeedUserVars();
    m_lua.RunBlock(m_initRef, "initCode");
    m_inited = true;
}

void Bump::SeedUserVars()
{
    const std::string all = Cfg.InitCode + "\n" + Cfg.FrameCode + "\n" + Cfg.BeatCode;
    for (const auto& v : LuaRuntime::ScanVarDecls(all, k_builtins))
        m_lua.SeedVar(v);
}

void Bump::Recompile()
{
    m_lua.CompileBlock(Cfg.InitCode,  "initCode",  m_initRef);
    m_lua.CompileBlock(Cfg.FrameCode, "frameCode", m_frameRef);
    m_lua.CompileBlock(Cfg.BeatCode,  "beatCode",  m_beatRef);
}

void Bump::OnConfigChanged(const std::vector<std::string>& Changed)
{
    if (!m_inited) return;

    bool codeChanged = false;
    for (const std::string& k : Changed)
        if (k == "initCode" || k == "frameCode" || k == "beatCode")
            { codeChanged = true; break; }

    if (!codeChanged) return;

    Recompile();
    SeedUserVars();
    m_lua.RunBlock(m_initRef, "initCode");
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
    if (Context.IsBeat() && Cfg.OnBeat)
    {
        m_curDepth      = Cfg.OnBeatDepth;
        m_onBeatFadeout = Cfg.OnBeatDuration;
    }
    else if (!m_onBeatFadeout)
    {
        m_curDepth = Cfg.Depth;
    }

    m_curDepth = (int)((double)m_curDepth * bi);
    const int depthScaled = (m_curDepth * 256) / 100;

    // Light center: normalized [0,1] → pixel coords (truncate, clamp to [0, dim]).
    const int centerX = std::clamp((int)(m_lua.GetEnvNumber("x") * w), 0, w);
    const int centerY = std::clamp((int)(m_lua.GetEnvNumber("y") * h), 0, h);

    const float texelSz[4] = { 1.0f / (float)w, 1.0f / (float)h, (float)w, (float)h };
    const float params[4]  = { (float)centerX, (float)centerY, (float)depthScaled, (float)Cfg.BlendMode };
    const float flags[4]   = { Cfg.InvertDepth  ? 1.0f : 0.0f,
                                Cfg.ShowLightPos ? 1.0f : 0.0f, 0.0f, 0.0f };

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
        if (m_onBeatFadeout > 0 && Cfg.OnBeatDuration > 0)
        {
            const int step = std::abs(Cfg.Depth - Cfg.OnBeatDepth) / Cfg.OnBeatDuration;
            m_curDepth += step * (Cfg.OnBeatDepth > Cfg.Depth ? -1 : 1);
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
