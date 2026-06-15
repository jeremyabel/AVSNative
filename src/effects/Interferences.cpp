#include "Interferences.h"
#include "engine/MathConstants.h"
#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_interferences.sc.bin.h"

#include <cmath>    

static constexpr const char* NAME_NPoints = "nPoints";
static constexpr const char* NAME_Alpha = "alpha";
static constexpr const char* NAME_Distance = "distance";
static constexpr const char* NAME_RotationInc = "rotationinc";
static constexpr const char* NAME_BeatAlpha = "alpha2";
static constexpr const char* NAME_BeatDistance = "distance2";
static constexpr const char* NAME_BeatRotationInc = "rotationinc2";
static constexpr const char* NAME_InitialRotation = "rotation";
static constexpr const char* NAME_Speed = "speed";
static constexpr const char* NAME_EnableOnBeatChange = "onbeat";
static constexpr const char* NAME_EnableRGB = "rgb";
static constexpr const char* NAME_ReverseRotation = "reverseRotation";
static constexpr const char* NAME_OutBlend = "outBlend";

void Interferences::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_interferences_spv, sizeof(fs_interferences_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    Offsets0Uniform = bgfx::createUniform("u_ifOffsets0", bgfx::UniformType::Vec4);
    Offsets1Uniform = bgfx::createUniform("u_ifOffsets1", bgfx::UniformType::Vec4);
    Offsets2Uniform = bgfx::createUniform("u_ifOffsets2", bgfx::UniformType::Vec4);
    Offsets3Uniform = bgfx::createUniform("u_ifOffsets3", bgfx::UniformType::Vec4);
    ParamsUniform = bgfx::createUniform("u_ifParams", bgfx::UniformType::Vec4);
}

void Interferences::Destroy()
{
    if (bgfx::isValid(ParamsUniform))
        bgfx::destroy(ParamsUniform);
    
    if (bgfx::isValid(Offsets3Uniform))
        bgfx::destroy(Offsets3Uniform);
    
    if (bgfx::isValid(Offsets2Uniform))
        bgfx::destroy(Offsets2Uniform);
    
    if (bgfx::isValid(Offsets1Uniform))
        bgfx::destroy(Offsets1Uniform);
    
    if (bgfx::isValid(Offsets0Uniform))
        bgfx::destroy(Offsets0Uniform);
    
    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    ParamsUniform = BGFX_INVALID_HANDLE;
    Offsets3Uniform = BGFX_INVALID_HANDLE;
    Offsets2Uniform = BGFX_INVALID_HANDLE;
    Offsets1Uniform = BGFX_INVALID_HANDLE;
    Offsets0Uniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

void Interferences::Render(const RenderContext& Context)
{
    // Beat: kick the oscillator if it has completed its previous cycle
    if (EnableOnBeatChange && Context.IsBeat() && Phase >= avs::Pi)
    {
        Phase = 0.0f;
    }

    const float SinPhase = std::sin(Phase);
    const float RotInc = RotationInc + (BeatRotationInc - RotationInc) * SinPhase;
    const float alpha = Alpha + (BeatAlpha - Alpha) * SinPhase;
    const float Dist = Distance + (BeatDistance - Distance) * SinPhase;

    // Radially-distributed UV-space offsets
    const float a0 = (Rotation / 255.0f) * 2.0f * avs::Pi;
    const float AngleStep = NPoints > 0 ? (2.0f * avs::Pi) / float(NPoints) : 0.0f;
    const float W = float(Context.Width);
    const float H = float(Context.Height);

    // Integer pixel offsets, exactly like the win32 original: xpoints[i] = (int)(cos(angle) * distance)
    // Truncating to whole pixels and dividing by the buffer size yields a
    // texel-aligned UV offset. Combined with point sampling in the shader, each
    // fetch reads one source texel with no bilinear blend — which is what keeps
    // the feedback loop bounded (fractional offsets + bilinear filtering act as a
    // spatial low-pass with gain >= 1 and make the buffer bloom/blow out).
    float Offsets[16] = {};
    for (int OffsetIdx = 0; OffsetIdx < NPoints && OffsetIdx < 8; ++OffsetIdx)
    {
        const float Angle = a0 + float(OffsetIdx) * AngleStep;
        const int IntX = (int)(std::cos(Angle) * Dist);
        const int IntY = (int)(std::sin(Angle) * Dist);
        Offsets[OffsetIdx * 2 + 0] = float(IntX) / W;
        Offsets[OffsetIdx * 2 + 1] = (ReverseRotation ? -1.0f : 1.0f) * float(IntY) / H;
    }

    Rotation += RotInc;
    if (Rotation >  255.0f)
        Rotation -= 255.0f;
    if (Rotation < -255.0f)
        Rotation += 255.0f;

    Phase += Speed;
    if (Phase > avs::Pi)
        Phase = avs::Pi;
    if (Phase < -avs::Pi)
        Phase = avs::Pi;

    // RGB mode is only active when nPoints is exactly 3 or 6
    const float rgbFlag = (EnableRGB && (NPoints == 3 || NPoints == 6)) ? 1.f : 0.f;

    // Pass alpha as an integer 0..255 — the shader does the multiply in integer pixel space (floor(v*alpha/255)) to match the win32 lut_u8_multiply table.
    const float uParams[4] = { float(NPoints), std::floor(alpha), rgbFlag, float(OutBlend) };

    bgfx::setUniform(Offsets0Uniform, Offsets + 0);
    bgfx::setUniform(Offsets1Uniform, Offsets + 4);
    bgfx::setUniform(Offsets2Uniform, Offsets + 8);
    bgfx::setUniform(Offsets3Uniform, Offsets + 12);
    bgfx::setUniform(ParamsUniform, uParams);
    bgfx::setTexture(0, TexUniform, Context.InputTexture, BGFX_SAMPLER_POINT);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

nlohmann::json Interferences::Serialize() const
{
    return 
    {
        { NAME_NPoints, NPoints },
        { NAME_Alpha, Alpha },
        { NAME_Distance, Distance },
        { NAME_RotationInc, RotationInc },
        { NAME_BeatAlpha, BeatAlpha },
        { NAME_BeatDistance, BeatDistance },
        { NAME_BeatRotationInc, BeatRotationInc },
        { NAME_InitialRotation, Rotation },
        { NAME_Speed, Speed },
        { NAME_EnableOnBeatChange, EnableOnBeatChange },
        { NAME_EnableRGB, EnableRGB },
        { NAME_ReverseRotation, ReverseRotation },
        { NAME_OutBlend, OutBlend },
    };
}

void Interferences::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, NAME_NPoints, NPoints);
    JsonUtil::ReadFloat(j, NAME_Alpha, Alpha);
    JsonUtil::ReadFloat(j, NAME_Distance, Distance);
    JsonUtil::ReadFloat(j, NAME_RotationInc, RotationInc);
    JsonUtil::ReadFloat(j, NAME_BeatAlpha, BeatAlpha);
    JsonUtil::ReadFloat(j, NAME_BeatDistance, BeatDistance);
    JsonUtil::ReadFloat(j, NAME_BeatRotationInc, BeatRotationInc);
    JsonUtil::ReadFloat(j, NAME_InitialRotation, Rotation);
    JsonUtil::ReadFloat(j, NAME_Speed, Speed);
    JsonUtil::ReadBool(j, NAME_EnableOnBeatChange, EnableOnBeatChange);
    JsonUtil::ReadBool(j, NAME_EnableRGB, EnableRGB);
    JsonUtil::ReadBool(j, NAME_ReverseRotation, ReverseRotation);
    JsonUtil::ReadInt(j, NAME_OutBlend, OutBlend);
}
