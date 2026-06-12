#pragma once

#include "engine/Effect.h"

class Blur : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int Intensity = 1; // index into {Light, Medium, Heavy}

    static constexpr const char* kIntensity = "intensity";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Blur"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    void EnsureScratch(uint16_t Width, uint16_t Height);
    void DestroyScratch();

    bgfx::FrameBufferHandle ScratchFBO = BGFX_INVALID_HANDLE;
    uint16_t ScratchW = 0;
    uint16_t ScratchH = 0;

    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
