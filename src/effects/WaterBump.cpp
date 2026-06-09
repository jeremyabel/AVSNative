#include "WaterBump.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_waterbump.sc.bin.h"

#include <cmath>
#include <cstdlib>
#include <algorithm>

void WaterBump::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_waterbump_spv, sizeof(fs_waterbump_spv)));
    Program    = bgfx::createProgram(VS, FS, true);
    InputUnif  = bgfx::createUniform("s_input",     bgfx::UniformType::Sampler);
    HeightUnif = bgfx::createUniform("s_height",    bgfx::UniformType::Sampler);
    TexelUnif  = bgfx::createUniform("u_texelSize", bgfx::UniformType::Vec4);
}

void WaterBump::EnsureBuffers(uint16_t W, uint16_t H)
{
    if (BufW == W && BufH == H)
        return;

    if (bgfx::isValid(HeightTex))
        bgfx::destroy(HeightTex);

    int n = (int)W * H;
    Bufs[0].assign(n, 0);
    Bufs[1].assign(n, 0);
    UploadBuf.resize(n);
    Page = 0;
    BufW = W;
    BufH = H;

    HeightTex = bgfx::createTexture2D(W, H, false, 1, bgfx::TextureFormat::R32F,
        BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT |
        BGFX_SAMPLER_U_CLAMP   | BGFX_SAMPLER_V_CLAMP);
}

// Stamps a sinusoidal radial disturbance into Bufs[Page]. Matches JS _sineBlob() exactly.
// Pass X<0 or Y<0 to randomize that coordinate.
void WaterBump::SineBlob(int X, int Y, int Radius, int Height)
{
    int* buf = Bufs[Page].data();
    const int w = BufW, bh = BufH;

    if (X < 0) X = 1 + Radius + std::rand() % std::max(1, w  - 2 * Radius - 1);
    if (Y < 0) Y = 1 + Radius + std::rand() % std::max(1, bh - 2 * Radius - 1);

    const int   radsq  = Radius * Radius;
    const float length = (1024.0f / Radius) * (1024.0f / Radius);

    int left = -Radius, right = Radius, top = -Radius, bottom = Radius;
    if (X - Radius < 1)         left   -= (X - Radius - 1);
    if (Y - Radius < 1)         top    -= (Y - Radius - 1);
    if (X + Radius > w  - 1)    right  -= (X + Radius - w  + 1);
    if (Y + Radius > bh - 1)    bottom -= (Y + Radius - bh + 1);

    for (int cy = top; cy < bottom; cy++)
    {
        for (int cx = left; cx < right; cx++)
        {
            int sq = cy * cy + cx * cx;
            if (sq < radsq)
            {
                float dist = std::sqrt((float)sq * length);
                // Matches original: ((cos(dist) + 0xffff) * height) >> 19
                buf[w * (Y + cy) + (X + cx)] += (int)((std::cos(dist) + 0xffff) * (float)Height) >> 19;
            }
        }
    }
}

// 2D wave equation step. Reads Bufs[Page], writes Bufs[Page^1]. Matches JS _calcWater() exactly.
void WaterBump::CalcWater()
{
    int32_t* oldbuf = Bufs[Page].data();
    int32_t* newbuf = Bufs[Page ^ 1].data();
    const int w  = BufW;
    const int h  = BufH;
    const int fl = Cfg.Fluidity;

    for (int y = 1; y < h - 1; y++)
    {
        for (int x = 1; x < w - 1; x++)
        {
            int i    = y * w + x;
            int newh = ((oldbuf[i + w]     + oldbuf[i - w]
                       + oldbuf[i + 1]     + oldbuf[i - 1]
                       + oldbuf[i - w - 1] + oldbuf[i - w + 1]
                       + oldbuf[i + w - 1] + oldbuf[i + w + 1]) >> 2)
                      - newbuf[i];
            newbuf[i] = newh - (newh >> fl);
        }
    }
}

void WaterBump::Render(const RenderContext& Context)
{
    const uint16_t w = (uint16_t)Context.Width;
    const uint16_t h = (uint16_t)Context.Height;

    EnsureBuffers(w, h);

    if (Context.IsBeat)
    {
        if (Cfg.Random)
        {
            const int maxDim  = std::max(w, h);
            const int radius  = Cfg.DropRadius * maxDim / 100;
            SineBlob(-1, -1, radius, -Cfg.Depth);
        }
        else
        {
            const int xPos[3] = { w / 4, w / 2, w * 3 / 4 };
            const int yPos[3] = { h / 4, h / 2, h * 3 / 4 };
            SineBlob(xPos[Cfg.DropPositionX], yPos[Cfg.DropPositionY],
                     Cfg.DropRadius, -Cfg.Depth);
        }
    }

    // Convert int32 height to float and upload to GPU texture.
    {
        const int32_t* src = Bufs[Page].data();
        float*         dst = UploadBuf.data();
        const int n = (int)w * h;
        for (int i = 0; i < n; i++)
            dst[i] = (float)src[i];

        const bgfx::Memory* mem = bgfx::copy(dst, (uint32_t)(n * sizeof(float)));
        bgfx::updateTexture2D(HeightTex, 0, 0, 0, 0, w, h, mem);
    }

    // GPU displacement pass.
    const float texelSize[4] = { 1.0f / (float)w, 1.0f / (float)h, 0.0f, 0.0f };
    bgfx::setUniform(TexelUnif, texelSize);
    bgfx::setTexture(0, InputUnif,  Context.InputTexture,
                     BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT);
    bgfx::setTexture(1, HeightUnif, HeightTex);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();

    // Advance simulation: reads Page, writes Page^1.
    CalcWater();
    Page ^= 1;
}

void WaterBump::Destroy()
{
    if (bgfx::isValid(HeightTex))  bgfx::destroy(HeightTex);
    if (bgfx::isValid(TexelUnif))  bgfx::destroy(TexelUnif);
    if (bgfx::isValid(HeightUnif)) bgfx::destroy(HeightUnif);
    if (bgfx::isValid(InputUnif))  bgfx::destroy(InputUnif);
    if (bgfx::isValid(Program))    bgfx::destroy(Program);

    HeightTex  = BGFX_INVALID_HANDLE;
    TexelUnif  = BGFX_INVALID_HANDLE;
    HeightUnif = BGFX_INVALID_HANDLE;
    InputUnif  = BGFX_INVALID_HANDLE;
    Program    = BGFX_INVALID_HANDLE;

    Bufs[0].clear();
    Bufs[1].clear();
    UploadBuf.clear();
    Page = 0;
    BufW = BufH = 0;
}
