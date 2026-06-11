#include "DynamicShift.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_dynamicshift.sc.bin.h"

#include <algorithm>

// Built-in names that ScanVarDecls must not treat as user-declared.
// x, y, alpha are the shader-driving outputs; w, h, b are per-frame inputs.
static const std::vector<std::string> k_builtins = {
    "x", "y", "alpha", "w", "h", "b", "getspec", "getosc"
};

void DynamicShift::RescanUserVars()
{
    auto vars = LuaRuntime::ScanVarDecls(Cfg.InitCode, k_builtins);
    for (const auto& v : vars)
        m_lua.SeedVar(v);
}

void DynamicShift::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_dynamicshift_spv, sizeof(fs_dynamicshift_spv)));
    Program = bgfx::createProgram(VS, FS, true);

    Params0Unif = bgfx::createUniform("u_ds_params0", bgfx::UniformType::Vec4);
    Params1Unif = bgfx::createUniform("u_ds_params1", bgfx::UniformType::Vec4);
    InputUnif   = bgfx::createUniform("s_input",      bgfx::UniformType::Sampler);

    // Seed all built-ins so arithmetic on them never errors.
    for (const auto& v : k_builtins)
        m_lua.SeedVar(v);
    m_lua.SetEnvNumber("x",     0.0);
    m_lua.SetEnvNumber("y",     0.0);
    m_lua.SetEnvNumber("alpha", 0.5);

    // Scan user-declared vars in init code (e.g. "d = 0" → seeds "d").
    RescanUserVars();

    m_lua.CompileBlock(Cfg.InitCode,  "initCode",  m_initRef);
    m_lua.CompileBlock(Cfg.FrameCode, "frameCode", m_frameRef);
    m_lua.CompileBlock(Cfg.BeatCode,  "beatCode",  m_beatRef);

    m_lua.SetEnvNumber("b", 0.0);
    m_lua.RunBlock(m_initRef, "initCode");
    m_inited = true;
}

void DynamicShift::Destroy()
{
    if (bgfx::isValid(Program))     bgfx::destroy(Program);
    if (bgfx::isValid(Params0Unif)) bgfx::destroy(Params0Unif);
    if (bgfx::isValid(Params1Unif)) bgfx::destroy(Params1Unif);
    if (bgfx::isValid(InputUnif))   bgfx::destroy(InputUnif);
    Program     = BGFX_INVALID_HANDLE;
    Params0Unif = BGFX_INVALID_HANDLE;
    Params1Unif = BGFX_INVALID_HANDLE;
    InputUnif   = BGFX_INVALID_HANDLE;
    m_inited    = false;
}

void DynamicShift::Render(const RenderContext& Context)
{
    if (!bgfx::isValid(Program)) return;

    m_lua.SetAudioData(Context.AudioData);
    m_lua.SetEnvNumber("b", Context.IsBeat() ? 1.0 : 0.0);
    m_lua.SetEnvNumber("w", (double)Context.Width);
    m_lua.SetEnvNumber("h", (double)Context.Height);

    m_lua.RunBlock(m_frameRef, "frameCode");
    if (Context.IsBeat())
        m_lua.RunBlock(m_beatRef, "beatCode");

    const double x     = m_lua.GetEnvNumber("x");
    const double y     = m_lua.GetEnvNumber("y");
    const double alpha = std::clamp(m_lua.GetEnvNumber("alpha"), 0.0, 1.0);

    // compat = original AVS 8-bit integer bilinear (only meaningful when Subpixel).
    const bool compat = Cfg.Subpixel && Cfg.Compat;

    const float p0[4] = { (float)x, (float)y, (float)Context.Width, (float)Context.Height };
    const float p1[4] = { Cfg.Blend ? 1.0f : 0.0f, (float)alpha, compat ? 1.0f : 0.0f, 0.0f };
    bgfx::setUniform(Params0Unif, p0);
    bgfx::setUniform(Params1Unif, p1);

    // Compat does its own integer texelFetch blend → bind POINT. Otherwise bilinear
    // when Subpixel, else nearest.
    uint32_t samplerFlags = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    if (!Cfg.Subpixel || compat)
        samplerFlags |= BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT;

    bgfx::setTexture(0, InputUnif, Context.InputTexture, samplerFlags);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void DynamicShift::OnConfigChanged(const std::vector<std::string>& Changed)
{
    if (!m_inited) return;

    for (const auto& k : Changed)
    {
        if (k == "initCode")
        {
            RescanUserVars();
            m_lua.CompileBlock(Cfg.InitCode, "initCode", m_initRef);
            m_lua.SetEnvNumber("b", 0.0);
            m_lua.RunBlock(m_initRef, "initCode");
        }
        else if (k == "frameCode") m_lua.CompileBlock(Cfg.FrameCode, "frameCode", m_frameRef);
        else if (k == "beatCode")  m_lua.CompileBlock(Cfg.BeatCode,  "beatCode",  m_beatRef);
    }
}
