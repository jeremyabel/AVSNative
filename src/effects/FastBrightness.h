#pragma once

#include "engine/Effect.h"

class FastBrightness : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int Dir = 0; // 0=×2 brighter, 1=×½ darker, 2=no change

    static constexpr const char* kDir = "dir";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Fast Brightness"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
