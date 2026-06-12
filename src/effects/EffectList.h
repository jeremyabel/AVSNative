#pragma once

#include "engine/Effect.h"
#include "engine/EffectChain.h"
#include "engine/FBOManager.h"
#include "engine/LuaRuntime.h"

#include <string>

class EffectList : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
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
    // Lua evaluation override: per-frame Init/Frame blocks that can read/write
    // enabled, beat, clear, alphain, alphaout (w/h read-only). Off by default.
    bool        UseEval  = false;
    std::string InitCode;
    std::string FrameCode;

    static constexpr const char* kOnBeat            = "onBeat";
    static constexpr const char* kOnBeatFrames      = "onBeatFrames";
    static constexpr const char* kClearFrame        = "clearFrame";
    static constexpr const char* kInBlend           = "inBlend";
    static constexpr const char* kInBlendBuf        = "inBlendBuf";
    static constexpr const char* kInBlendBufInvert  = "inBlendBufInvert";
    static constexpr const char* kOutBlend          = "outBlend";
    static constexpr const char* kOutBlendBuf       = "outBlendBuf";
    static constexpr const char* kOutBlendBufInvert = "outBlendBufInvert";
    static constexpr const char* kBlendAmt          = "blendAmt";
    static constexpr const char* kUseEval           = "useCode";
    static constexpr const char* kInitCode          = "initCode";
    static constexpr const char* kFrameCode         = "frameCode";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Effect List"; }
    // Serializes only this list's own params. The inner chain is appended as
    // config["effects"] by Preset's SerialiseChain (via GetInnerChain), and
    // populated on load by Preset's LoadChain recursion.
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    EffectChain* GetInnerChain() override { return &Inner; }
    uint8_t ExpectedViewCount() const override;

    std::string GetScriptError(const std::string& paramName) const override
    {
        return m_lua.GetError(paramName);
    }

    // Recompile entry points for the eval override. Called after Deserialize and
    // by the UI when the matching code editor changes.
    void RecompileInitCode();   // rescans user vars; Init re-runs on next active frame
    void RecompileFrameCode();

private:
    void SubmitBlend(uint8_t ViewId,
                     bgfx::TextureHandle Base,
                     bgfx::TextureHandle Src,
                     bgfx::TextureHandle Mask,
                     int32_t Mode, float Amt, bool MaskInvert,
                     bgfx::VertexBufferHandle QuadVB);

    void EnsureInternalBuffers(const RenderContext& Context);

    // Blit input → output unchanged (used when on-beat window expired or the eval
    // override sets enabled=0). Allocates one 4-view block, like an inactive list.
    void RenderPassThrough(const RenderContext& Context);

    // Lua evaluation override
    void RescanUserVars();

    LuaRuntime m_lua;
    int  m_initRef   = -1;
    int  m_frameRef  = -1;
    bool m_inited    = false;  // Init() has set up Lua + compiled blocks
    bool m_needInit  = true;   // run the Init block on the next active eval frame

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
