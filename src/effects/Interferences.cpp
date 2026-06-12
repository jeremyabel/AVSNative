#include "Interferences.h"
#include "engine/MathConstants.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_interferences.sc.bin.h"

#include <cmath>


void Interferences::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_interferences_spv, sizeof(fs_interferences_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    Offsets0 = bgfx::createUniform("u_ifOffsets0", bgfx::UniformType::Vec4);
    Offsets1 = bgfx::createUniform("u_ifOffsets1", bgfx::UniformType::Vec4);
    Offsets2 = bgfx::createUniform("u_ifOffsets2", bgfx::UniformType::Vec4);
    Offsets3 = bgfx::createUniform("u_ifOffsets3", bgfx::UniformType::Vec4);
    ParamsUniform = bgfx::createUniform("u_ifParams", bgfx::UniformType::Vec4);
}

void Interferences::Render(const RenderContext& Context)
{
    // Beat: kick the oscillator if it has completed its previous cycle
    if (Context.IsBeat() && OnBeat && Status >= avs::Pi)
    {
        Status = 0.0f;
    }

    const float s = std::sin(Status);
    const float rotInc = RotationInc + (RotationInc2 - RotationInc) * s;
    const float alpha = Alpha + (Alpha2 - Alpha) * s;
    const float dist = Distance + (Distance2 - Distance) * s;

    // Radially-distributed UV-space offsets
    const float a0 = (Rotation / 255.0f) * 2.0f * avs::Pi;
    const float angleStep = NPoints > 0 ? (2.0f * avs::Pi) / float(NPoints) : 0.0f;
    const float fw = float(Context.Width);
    const float fh = float(Context.Height);

    // The v component is negated to compensate for bgfx's vs_fullscreen putting
    // v=0 at screen-top (screen-up = -v), vs WebGL's vUv.y up = screen-up. This
    // makes the rotation direction match the win32 original. Unchecking
    // ReverseRotation drops the negation to match the AVSWeb JS reference instead.
    const float vSign = ReverseRotation ? -1.0f : 1.0f;

    // Integer pixel offsets, exactly like the win32 original:
    //   xpoints[i] = (int)(cos(angle) * distance)
    // Truncating to whole pixels and dividing by the buffer size yields a
    // texel-aligned UV offset. Combined with point sampling in the shader, each
    // fetch reads one source texel with no bilinear blend — which is what keeps
    // the feedback loop bounded (fractional offsets + bilinear filtering act as a
    // spatial low-pass with gain >= 1 and make the buffer bloom/blow out).
    float offsets[16] = {};
    for (int i = 0; i < NPoints && i < 8; ++i)
    {
        const float a = a0 + float(i) * angleStep;
        const int ipx = (int)(std::cos(a) * dist);   // truncate toward zero (matches (int) cast)
        const int ipy = (int)(std::sin(a) * dist);
        offsets[i * 2 + 0] = float(ipx) / fw;
        offsets[i * 2 + 1] = vSign * float(ipy) / fh;
    }

    // Advance rotation (matches JS single-step wrap)
    Rotation += rotInc;
    if (Rotation >  255.0f)
        Rotation -= 255.0f;
    if (Rotation < -255.0f)
        Rotation += 255.0f;

    // Advance oscillation phase
    Status += Speed;
    if (Status > avs::Pi)
        Status = avs::Pi;
    if (Status < -avs::Pi)
        Status = avs::Pi;

    // rgb mode is only active when nPoints is exactly 3 or 6
    const int rgbFlag = (RGB && (NPoints == 3 || NPoints == 6)) ? 1 : 0;

    // Pass alpha as an integer 0..255 — the shader does the multiply in integer
    // pixel space (floor(v*alpha/255)) to match the win32 lut_u8_multiply table.
    const float alpha255 = std::floor(alpha);
    const float Params[4] = { float(NPoints), alpha255, float(rgbFlag), float(OutBlend) };

    bgfx::setUniform(Offsets0, offsets + 0);
    bgfx::setUniform(Offsets1, offsets + 4);
    bgfx::setUniform(Offsets2, offsets + 8);
    bgfx::setUniform(Offsets3, offsets + 12);
    bgfx::setUniform(ParamsUniform, Params);
    // Point sampling: feedback must read exact texels (see offset comment above).
    bgfx::setTexture(0, TexUniform, Context.InputTexture, BGFX_SAMPLER_POINT);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void Interferences::Destroy()
{
    if (bgfx::isValid(ParamsUniform))
        bgfx::destroy(ParamsUniform);
    
    if (bgfx::isValid(Offsets3))
        bgfx::destroy(Offsets3);
    
    if (bgfx::isValid(Offsets2))
        bgfx::destroy(Offsets2);
    
    if (bgfx::isValid(Offsets1))
        bgfx::destroy(Offsets1);
    
    if (bgfx::isValid(Offsets0))
        bgfx::destroy(Offsets0);
    
    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    ParamsUniform = BGFX_INVALID_HANDLE;
    Offsets3 = BGFX_INVALID_HANDLE;
    Offsets2 = BGFX_INVALID_HANDLE;
    Offsets1 = BGFX_INVALID_HANDLE;
    Offsets0 = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

nlohmann::json Interferences::Serialize() const
{
    return {
        { kNPoints,         NPoints         },
        { kAlpha,           Alpha           },
        { kDistance,        Distance        },
        { kRotationInc,     RotationInc     },
        { kAlpha2,          Alpha2          },
        { kDistance2,       Distance2       },
        { kRotationInc2,    RotationInc2    },
        { kRotation,        Rotation        },
        { kSpeed,           Speed           },
        { kOnBeat,          OnBeat          },
        { kRGB,             RGB             },
        { kReverseRotation, ReverseRotation },
        { kOutBlend,        OutBlend        },
    };
}

void Interferences::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt  (j, kNPoints,         NPoints);
    JsonUtil::ReadFloat(j, kAlpha,           Alpha);
    JsonUtil::ReadFloat(j, kDistance,        Distance);
    JsonUtil::ReadFloat(j, kRotationInc,     RotationInc);
    JsonUtil::ReadFloat(j, kAlpha2,          Alpha2);
    JsonUtil::ReadFloat(j, kDistance2,       Distance2);
    JsonUtil::ReadFloat(j, kRotationInc2,    RotationInc2);
    JsonUtil::ReadFloat(j, kRotation,        Rotation);
    JsonUtil::ReadFloat(j, kSpeed,           Speed);
    JsonUtil::ReadBool (j, kOnBeat,          OnBeat);
    JsonUtil::ReadBool (j, kRGB,             RGB);
    JsonUtil::ReadBool (j, kReverseRotation, ReverseRotation);
    JsonUtil::ReadInt  (j, kOutBlend,        OutBlend);
}
