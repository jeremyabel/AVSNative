#pragma once

#include "engine/Effect.h"

class BufferSave : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int   Mode      = 0;     // 0=Save, 1=Restore, 2=Alt Save/Restore, 3=Alt Restore/Save
    int   Slot      = 0;     // scratch buffer index 0–7
    int   BlendMode = 0;     // 0=Replace … 11=XOR (matches shader constants)
    float BlendAmt  = 0.5f;  // for BlendMode==6 (Adjustable)

    static constexpr const char* kMode      = "mode";
    static constexpr const char* kSlot      = "slot";
    static constexpr const char* kBlendMode = "blendMode";
    static constexpr const char* kBlendAmt  = "blendAmt";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Buffer Save"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    void DoSave(const RenderContext& Context);
    void DoRestore(const RenderContext& Context);
    void SubmitBlit(uint8_t ViewId, bgfx::TextureHandle Tex, bgfx::VertexBufferHandle QuadVB);
    void SubmitBlend(uint8_t ViewId, bgfx::TextureHandle Base, bgfx::TextureHandle Src, bgfx::VertexBufferHandle QuadVB);

    bool AltPhase = false;

    bgfx::ProgramHandle BlitProgram  = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle BlendProgram = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle BlitTexUniform     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle BaseTexUniform     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle SrcTexUniform      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle BlendParamsUniform = BGFX_INVALID_HANDLE;
};
