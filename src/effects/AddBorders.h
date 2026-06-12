#pragma once

#include "engine/Effect.h"

#include <array>

class AddBorders : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    std::array<uint8_t, 3> Color = { 0, 0, 0 };
    int Size = 1; // 1–50, percentage of each dimension

    static constexpr const char* kColor = "color";
    static constexpr const char* kSize  = "size";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Add Borders"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle BorderParamsUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle BorderColorUniform = BGFX_INVALID_HANDLE;
};
