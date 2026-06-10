#pragma once

#include "engine/Reflect.h"

struct BufferSaveConfig
{
    int   Mode      = 0;     // 0=Save, 1=Restore, 2=Alt Save/Restore, 3=Alt Restore/Save
    int   Slot      = 0;     // scratch buffer index 0–7
    int   BlendMode = 0;     // 0=Replace … 11=XOR (matches shader constants)
    float BlendAmt  = 0.5f;  // for BlendMode==6 (Adjustable)
};

class BufferSave : public ReflectedEffect<BufferSaveConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            SelectI(&BufferSaveConfig::Mode, "mode", "Mode",
                    { "Save", "Restore", "Alternate Save/Restore", "Alternate Restore/Save" }),
            SelectI(&BufferSaveConfig::Slot, "slot", "Buffer Slot",
                    { "0", "1", "2", "3", "4", "5", "6", "7" }),
            SelectI(&BufferSaveConfig::BlendMode, "blendMode", "Blend",
                    { "Replace", "Additive", "Maximum", "50/50", "Multiply",
                      "Subtractive 1", "Adjustable", "Minimum",
                      "Every Other Pixel", "Every Other Line", "Subtractive 2", "XOR" }),
            Range(&BufferSaveConfig::BlendAmt, "blendAmt", "Blend Amount", 0.0f, 1.0f, 0.01f),
        };
        return f;
    }
    std::string EffectName() const override { return "Buffer Save"; }

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
