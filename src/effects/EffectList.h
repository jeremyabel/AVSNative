#pragma once

#include "engine/Reflect.h"
#include "engine/EffectChain.h"
#include "engine/FBOManager.h"

struct EffectListConfig
{
    int  InBlend    = 0;     // 0=Ignore
    int  OutBlend   = 1;     // 1=Replace
    bool ClearFrame = false;
    float BlendAmt  = 0.5f;
    int  InBlendBuf  = 0;
    int  OutBlendBuf = 0;
    bool InBlendBufInvert  = false;
    bool OutBlendBufInvert = false;
    bool OnBeat       = false;
    int  OnBeatFrames = 1;
};

class EffectList : public ReflectedEffect<EffectListConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    // EffectList augments the generic field JSON with its inner chain.
    nlohmann::json GetConfig() const override;

    EffectChain* GetInnerChain() override { return &Inner; }
    uint8_t ExpectedViewCount() const override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<std::string> kBlendOpts = {
            "Ignore", "Replace", "50/50", "Maximum", "Additive",
            "Subtractive 1", "Subtractive 2", "Every Other Line", "Every Other Pixel",
            "XOR", "Adjustable", "Multiply", "Buffer", "Minimum"
        };
        static const std::vector<std::string> kSlotOpts = { "0","1","2","3","4","5","6","7" };

        static const std::vector<Field> f = {
            Bool(&EffectListConfig::OnBeat, "onBeat", "Enable on Beat"),
            NumberI(&EffectListConfig::OnBeatFrames, "onBeatFrames", "For N Frames", 0, 64),
            Bool(&EffectListConfig::ClearFrame, "clearFrame", "Clear Frame"),
            SelectI(&EffectListConfig::InBlend, "inBlend", "Input Blend", kBlendOpts),
            SelectI(&EffectListConfig::InBlendBuf, "inBlendBuf", "Input Buffer", kSlotOpts),
            Bool(&EffectListConfig::InBlendBufInvert, "inBlendBufInvert", "Invert Input Mask"),
            SelectI(&EffectListConfig::OutBlend, "outBlend", "Output Blend", kBlendOpts),
            SelectI(&EffectListConfig::OutBlendBuf, "outBlendBuf", "Output Buffer", kSlotOpts),
            Bool(&EffectListConfig::OutBlendBufInvert, "outBlendBufInvert", "Invert Output Mask"),
            Range(&EffectListConfig::BlendAmt, "blendAmt", "Blend Amount", 0.0f, 1.0f, 0.01f),
        };
        return f;
    }
    std::string EffectName() const override { return "Effect List"; }

private:
    void SubmitBlend(uint8_t ViewId,
                     bgfx::TextureHandle Base,
                     bgfx::TextureHandle Src,
                     bgfx::TextureHandle Mask,
                     int32_t Mode, float Amt, bool MaskInvert,
                     bgfx::VertexBufferHandle QuadVB);

    void EnsureInternalBuffers(const RenderContext& Context);

    // Sub-effect chain rendered in isolation
    EffectChain Inner;

    // Isolated ping-pong for sub-effects; persists across frames
    InnerFBOManager InnerFbos;
    bool            InnerFbosReady = false;

    // Runtime state
    int32_t OnBeatCooldown = 0;

    // Blend program resources
    bgfx::ProgramHandle BlendProgram = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle BaseTexUniform      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle SrcTexUniform       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle MaskTexUniform      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle BlendParamsUniform  = BGFX_INVALID_HANDLE;

};
