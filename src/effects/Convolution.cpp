#include "effects/Convolution.h"
#include "engine/FBOManager.h"

#include <bgfx/bgfx.h>

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_convolution.sc.bin.h"

#include <algorithm>

// ── GetConfig / SetConfig ─────────────────────────────────────────────────────

nlohmann::json Convolution::GetConfig() const
{
    nlohmann::json j = ReflectedEffect<ConvolutionConfig>::GetConfig();
    j["kernel"] = Cfg.Kernel;   // std::array<int,49> -> JSON array
    return j;
}

void Convolution::SetConfig(const nlohmann::json& cfg)
{
    ReflectedEffect<ConvolutionConfig>::SetConfig(cfg);

    if (cfg.contains("kernel") && cfg["kernel"].is_array())
    {
        const auto& arr = cfg["kernel"];
        for (size_t i = 0; i < Cfg.Kernel.size(); ++i)
            Cfg.Kernel[i] = (i < arr.size() && arr[i].is_number())
                                ? arr[i].get<int>() : 0;
    }
}

void Convolution::OnConfigChanged(const std::vector<std::string>& changed)
{
    // Wrap and Absolute are mutually exclusive (matches the reference).
    const auto has = [&](const char* k) {
        return std::find(changed.begin(), changed.end(), k) != changed.end();
    };
    if (has("wrap") && Cfg.Wrap)         Cfg.Absolute = false;
    if (has("absolute") && Cfg.Absolute) Cfg.Wrap     = false;
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

void Convolution::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv,   sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_convolution_spv,  sizeof(fs_convolution_spv)));
    m_prog = bgfx::createProgram(VS, FS, true);

    m_inputUnif   = bgfx::createUniform("s_input",       bgfx::UniformType::Sampler);
    m_kernelUnif  = bgfx::createUniform("u_kernel",      bgfx::UniformType::Vec4, 13);
    m_paramsUnif  = bgfx::createUniform("u_convParams",  bgfx::UniformType::Vec4);
    m_params2Unif = bgfx::createUniform("u_convParams2", bgfx::UniformType::Vec4);
}

void Convolution::Destroy()
{
    if (bgfx::isValid(m_params2Unif)) bgfx::destroy(m_params2Unif);
    if (bgfx::isValid(m_paramsUnif))  bgfx::destroy(m_paramsUnif);
    if (bgfx::isValid(m_kernelUnif))  bgfx::destroy(m_kernelUnif);
    if (bgfx::isValid(m_inputUnif))   bgfx::destroy(m_inputUnif);
    if (bgfx::isValid(m_prog))        bgfx::destroy(m_prog);

    m_params2Unif = BGFX_INVALID_HANDLE;
    m_paramsUnif  = BGFX_INVALID_HANDLE;
    m_kernelUnif  = BGFX_INVALID_HANDLE;
    m_inputUnif   = BGFX_INVALID_HANDLE;
    m_prog        = BGFX_INVALID_HANDLE;
}

// ── Render ────────────────────────────────────────────────────────────────────

void Convolution::Render(const RenderContext& Ctx)
{
    // Pack 49 kernel ints into 13 vec4s.
    float kernelData[13 * 4] = {};
    for (int i = 0; i < 49; ++i)
        kernelData[i] = (float)Cfg.Kernel[i];

    // Signed scale (sign handled in-shader); 0 is treated as 1 like the original.
    const float scale = (Cfg.Scale == 0) ? 1.0f : (float)Cfg.Scale;

    // The shader works in integer pixel units (0..255), so bias enters as the
    // original's 256*bias word value.
    float params[4] = {
        256.0f * (float)Cfg.Bias,
        scale,
        Cfg.Wrap     ? 1.0f : 0.0f,
        Cfg.Absolute ? 1.0f : 0.0f,
    };
    float params2[4] = {
        Cfg.TwoPass ? 1.0f : 0.0f,
        (Ctx.Width  > 0) ? 1.0f / (float)Ctx.Width  : 0.0f,
        (Ctx.Height > 0) ? 1.0f / (float)Ctx.Height : 0.0f,
        0.0f,
    };

    bgfx::setTexture(0, m_inputUnif, Ctx.InputTexture);
    bgfx::setUniform(m_kernelUnif, kernelData, 13);
    bgfx::setUniform(m_paramsUnif, params);
    bgfx::setUniform(m_params2Unif, params2);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Ctx.QuadVB);
    bgfx::submit(Ctx.ViewId, m_prog);

    Ctx.FboManager->Swap();
}
