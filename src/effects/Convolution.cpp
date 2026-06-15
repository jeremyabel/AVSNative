#include "effects/Convolution.h"
#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include <bgfx/bgfx.h>

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_convolution.sc.bin.h"

#include <algorithm>

static constexpr const char* NAME_EnableWrap = "wrap";
static constexpr const char* NAME_EnableAbsolute = "absolute";
static constexpr const char* NAME_EnableTwoPass = "twoPass";
static constexpr const char* NAME_Bias = "bias";
static constexpr const char* NAME_Scale = "scale";
static constexpr const char* NAME_Kernel = "kernel";

void Convolution::Init()
{
    bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_convolution_spv, sizeof(fs_convolution_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);
    KernelUniform = bgfx::createUniform("u_kernel", bgfx::UniformType::Vec4, 13);
    Params1Uniform = bgfx::createUniform("u_convParams", bgfx::UniformType::Vec4);
    Params2Uniform = bgfx::createUniform("u_convParams2", bgfx::UniformType::Vec4);
}

void Convolution::Destroy()
{
    if (bgfx::isValid(Params2Uniform)) 
        bgfx::destroy(Params2Uniform);

    if (bgfx::isValid(Params1Uniform))  
        bgfx::destroy(Params1Uniform);

    if (bgfx::isValid(KernelUniform))  
        bgfx::destroy(KernelUniform);

    if (bgfx::isValid(TexUniform))   
        bgfx::destroy(TexUniform);

    if (bgfx::isValid(Program))        
        bgfx::destroy(Program);

    Params2Uniform = BGFX_INVALID_HANDLE;
    Params1Uniform = BGFX_INVALID_HANDLE;
    KernelUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

void Convolution::Render(const RenderContext& Context)
{
    // Pack 49 kernel ints into 13 vec4s.
    float KernelData[13 * 4] = {};
    for (int i = 0; i < 49; ++i)
    {
        KernelData[i] = (float)Kernel[i];
    }

    // Signed scale (sign handled in-shader); 0 is treated as 1 like the original.
    const float scale = (Scale == 0) ? 1.0f : (float)Scale;

    // The shader works in integer pixel units (0..255), so bias enters as the
    // original's 256*bias word value.
    float uParams1[4] = { 256.0f * (float)Bias, scale, EnableWrap ? 1.0f : 0.0f, EnableAbsolute ? 1.0f : 0.0f };
    float uParams2[4] = {
        EnableTwoPass ? 1.0f : 0.0f,
        (Context.Width > 0) ? 1.0f / (float)Context.Width : 0.0f,
        (Context.Height > 0) ? 1.0f / (float)Context.Height : 0.0f,
        0.0f,
    };

    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setUniform(KernelUniform, KernelData, 13);
    bgfx::setUniform(Params1Uniform, uParams1);
    bgfx::setUniform(Params2Uniform, uParams2);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

nlohmann::json Convolution::Serialize() const
{
    return 
    {
        { NAME_EnableWrap, EnableWrap },
        { NAME_EnableAbsolute, EnableAbsolute },
        { NAME_EnableTwoPass, EnableTwoPass },
        { NAME_Bias, Bias },
        { NAME_Scale, Scale },
        { NAME_Kernel, Kernel },
    };
}

void Convolution::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadBool(j, NAME_EnableWrap, EnableWrap);
    JsonUtil::ReadBool(j, NAME_EnableAbsolute, EnableAbsolute);
    JsonUtil::ReadBool(j, NAME_EnableTwoPass, EnableTwoPass);
    JsonUtil::ReadInt(j, NAME_Bias, Bias);
    JsonUtil::ReadInt(j, NAME_Scale, Scale);

    if (j.contains(NAME_Kernel) && j[NAME_Kernel].is_array())
    {
        const auto& arr = j[NAME_Kernel];
        for (size_t i = 0; i < Kernel.size(); ++i)
        {
            Kernel[i] = (i < arr.size() && arr[i].is_number()) ? arr[i].get<int>() : 0;
        }
    }
}
