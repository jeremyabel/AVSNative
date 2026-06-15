#include "Starfield.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_blit.sc.bin.h"
#include "generated/spirv/vs_starfield.sc.bin.h"
#include "generated/spirv/fs_starfield.sc.bin.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>

static const float k_quadVerts[] = {
    -1.0f, -1.0f,
    -1.0f,  3.0f,
     3.0f, -1.0f,
};

// ─── colour formula ───────────────────────────────────────────────────────────
// Matches blend_adjustable_rough() from e_starfield.cpp.
// Inputs and output are in [0, 240] — guaranteed not to overflow uint8_t.
void Starfield::Colorize(uint8_t Bright, uint8_t Cr, uint8_t Cg, uint8_t Cb, uint8_t& OutR, uint8_t& OutG, uint8_t& OutB)
{
    int v = (Bright >> 4) & 0xF;
    int gn = v;
    OutR = (uint8_t)((gn * (16 - v)) + ((Cr >> 4) * v));
    OutG = (uint8_t)((gn * (16 - v)) + ((Cg >> 4) * v));
    OutB = (uint8_t)((gn * (16 - v)) + ((Cb >> 4) * v));
}

void Starfield::Init()
{
    bgfx::ShaderHandle FullscreenVertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle BlitFragShader = bgfx::createShader(bgfx::copy(fs_blit_spv, sizeof(fs_blit_spv)));
    BlitProgram = bgfx::createProgram(FullscreenVertShader, BlitFragShader, true);

    bgfx::ShaderHandle StarVertShader = bgfx::createShader(bgfx::copy(vs_starfield_spv, sizeof(vs_starfield_spv)));
    bgfx::ShaderHandle StarFragShader = bgfx::createShader(bgfx::copy(fs_starfield_spv, sizeof(fs_starfield_spv)));
    StarProgram = bgfx::createProgram(StarVertShader, StarFragShader, true);

    BlitTexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

    // Blit fullscreen triangle
    bgfx::VertexLayout BlitLayout;
    BlitLayout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .end();
    BlitQuadVB = bgfx::createVertexBuffer(bgfx::copy(k_quadVerts, sizeof(k_quadVerts)), BlitLayout);

    // Star point layout: position + normalised uint8 colour
    StarLayout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();
}

void Starfield::Destroy()
{
    if (bgfx::isValid(BlitQuadVB))     
        bgfx::destroy(BlitQuadVB);

    if (bgfx::isValid(BlitTexUniform)) 
        bgfx::destroy(BlitTexUniform);

    if (bgfx::isValid(StarProgram))    
        bgfx::destroy(StarProgram);
    
    if (bgfx::isValid(BlitProgram))    
        bgfx::destroy(BlitProgram);

    BlitQuadVB = BGFX_INVALID_HANDLE;
    BlitTexUniform = BGFX_INVALID_HANDLE;
    StarProgram = BGFX_INVALID_HANDLE;
    BlitProgram = BGFX_INVALID_HANDLE;
}

void Starfield::InitStars(int W, int H)
{
    // Scale star count to screen area, capped at kMaxStars-1
    AbsStars = std::min(kMaxStars - 1, (int)std::round((double)StarCount * W * H / (512.0 * 384.0)));

    int XOff = W >> 1;
    int YOff = H >> 1;
    for (int i = 0; i < AbsStars; i++)
    {
        Stars[i].X = (float)(rand() % W) - XOff;
        Stars[i].Y = (float)(rand() % H) - YOff;
        Stars[i].Z = (float)(rand() % 256);
        Stars[i].SpeedMult = (float)((rand() % 9) + 1) / 10.0f;
    }
}

void Starfield::ResetStar(int Idx, int W, int H, int XOff, int YOff)
{
    Stars[Idx].X = (float)(rand() % W) - XOff;
    Stars[Idx].Y = (float)(rand() % H) - YOff;
    Stars[Idx].Z = 255.0f;
}

void Starfield::Render(const RenderContext& Context)
{
    const int W = Context.Width;
    const int H = Context.Height;
    const int XOff = W >> 1;
    const int YOff = H >> 1;

    // On-beat: snap to on-beat speed and begin linear ramp back.
    if (Context.IsBeat() && OnBeat)
    {
        CurrentSpeed = OnBeatSpeed;
        OnBeatDiff = (Speed - OnBeatSpeed) / (float)OnBeatDuration;
        Cooldown = OnBeatDuration;
    }

    // Reinitialise pool whenever canvas size changes.
    if (W != LastW || H != LastH)
    {
        LastW = W;  LastH = H;
        CurrentSpeed = Speed;
        InitStars(W, H);
    }

    // Pass 1 (ViewId): blit input to output
    bgfx::setTexture(0, BlitTexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, BlitQuadVB);
    bgfx::submit(Context.ViewId, BlitProgram);

    // Simulate stars, build transient vertex buffer
    if (AbsStars > 0 && bgfx::getAvailTransientVertexBuffer((uint32_t)AbsStars, StarLayout) >= (uint32_t)AbsStars)
    {
        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, (uint32_t)AbsStars, StarLayout);
        StarVertex* verts = reinterpret_cast<StarVertex*>(tvb.data);
        int32_t count = 0;

        bool isWhite = (Color[0] == 255 && Color[1] == 255 && Color[2] == 255);
        // 50/50 uses alpha-blend (src_alpha, inv_src_alpha); encode alpha = 0.5.
        uint8_t alpha = (BlendMode == 2) ? 128 : 255;
        float curSpeed = CurrentSpeed;

        for (int i = 0; i < AbsStars; i++)
        {
            Star& s = Stars[i];

            if ((int)s.Z <= 0) { ResetStar(i, W, H, XOff, YOff); continue; }

            int nx = (int)(s.X * 128.0f / s.Z + (float)XOff);
            int ny = (int)(s.Y * 128.0f / s.Z + (float)YOff);

            if (nx <= 0 || nx >= W || ny <= 0 || ny >= H)
            {
                ResetStar(i, W, H, XOff, YOff);
                continue;
            }

            // Brightness increases as z → 0 (star approaches camera).
            uint8_t bright = (uint8_t)std::min(255, (int)((255.0f - (float)((int)s.Z)) * s.SpeedMult));

            uint8_t r, g, b;
            if (isWhite) { r = g = b = bright; }
            else         { Colorize(bright, Color[0], Color[1], Color[2], r, g, b); }

            // NDC: Vulkan y=-1 is top-of-screen, matching our UV convention.
            StarVertex& v = verts[count++];
            v.X = (float)nx / (float)W * 2.0f - 1.0f;
            v.Y = (float)ny / (float)H * 2.0f - 1.0f;
            v.R = r;  v.G = g;  v.B = b;  v.A = alpha;

            s.Z -= s.SpeedMult * curSpeed;
        }

        // Speed ramp-back (runs after star loop, matching original).
        if (Cooldown <= 0)
        {
            CurrentSpeed = Speed;
        }
        else
        {
            CurrentSpeed = std::max(0.0f, CurrentSpeed + OnBeatDiff);
            --Cooldown;
        }

        // Pass 2 (ViewId+1): draw stars on top of blitted input
        if (count > 0)
        {
            uint8_t starView = Context.ViewId + 1;
            bgfx::setViewFrameBuffer(starView, Context.OutputFBO);
            bgfx::setViewClear(starView, BGFX_CLEAR_NONE, 0);
            bgfx::setViewRect(starView, 0, 0, (uint16_t)W, (uint16_t)H);

            uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_PT_POINTS;
            switch (BlendMode)
            {
                case 1: state |= BGFX_STATE_BLEND_ADD; break;
                case 2: state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA); break;
                default: break; // Replace — no blending, overwrite pixel
            }

            bgfx::setState(state);
            bgfx::setVertexBuffer(0, &tvb, 0, (uint32_t)count);
            bgfx::submit(starView, StarProgram);
        }
    }
    else
    {
        // No transient space or no stars: speed ramp still ticks.
        if (Cooldown <= 0) 
        {
            CurrentSpeed = Speed;
        }
        else 
        { 
            CurrentSpeed = std::max(0.0f, CurrentSpeed + OnBeatDiff); 
            --Cooldown; 
        }
    }

    Context.FboManager->Swap();
}

nlohmann::json Starfield::Serialize() const
{
    return 
    {
        { NAME_Color, JsonUtil::ColorToJson(Color) },
        { NAME_BlendMode, BlendMode },
        { NAME_Speed, Speed },
        { NAME_StarCount, StarCount },
        { NAME_EnableOnBeatChange, OnBeat },
        { NAME_OnBeatSpeed, OnBeatSpeed },
        { NAME_OnBeatDuration, OnBeatDuration },
    };
}

void Starfield::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadColor(j, NAME_Color, Color);
    JsonUtil::ReadInt(j, NAME_BlendMode, BlendMode);
    JsonUtil::ReadFloat(j, NAME_Speed, Speed);
    JsonUtil::ReadInt(j, NAME_StarCount, StarCount);
    JsonUtil::ReadBool(j, NAME_EnableOnBeatChange, OnBeat);
    JsonUtil::ReadFloat(j, NAME_OnBeatSpeed, OnBeatSpeed);
    JsonUtil::ReadInt(j, NAME_OnBeatDuration, OnBeatDuration);

    ResetSpeed();
    ReinitStars();
}
