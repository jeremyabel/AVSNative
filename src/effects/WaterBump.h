#pragma once

#include "engine/Effect.h"

#include <cstdint>
#include <vector>

class WaterBump : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int  Fluidity      = 6;    // 2–10
    int  Depth         = 600;  // 100–2000
    bool Random        = false;
    int  DropPositionX = 1;    // 0=left  1=center  2=right
    int  DropPositionY = 1;    // 0=top   1=center  2=bottom
    int  DropRadius    = 40;   // 10–100

    static constexpr const char* kFluidity      = "fluidity";
    static constexpr const char* kDepth         = "depth";
    static constexpr const char* kRandom        = "random";
    static constexpr const char* kDropPositionX = "dropPositionX";
    static constexpr const char* kDropPositionY = "dropPositionY";
    static constexpr const char* kDropRadius    = "dropRadius";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Water Bump"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    void EnsureBuffers(uint16_t W, uint16_t H);
    void SineBlob(int X, int Y, int Radius, int Height);
    void CalcWater();

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle HeightUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexelSizeUnifiorm = BGFX_INVALID_HANDLE;  // u_texelSize: (1/w, 1/h, 0, 0)
    bgfx::TextureHandle HeightTex = BGFX_INVALID_HANDLE;

    std::vector<int32_t> Bufs[2];  // ping-pong CPU height buffers (int32, row-major, row 0 = top)
    std::vector<float> UploadBuf;
    int Page = 0;
    uint16_t BufW = 0;
    uint16_t BufH = 0;
};
