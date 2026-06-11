#include "EffectList.h"

#include "engine/FBOManager.h"
#include "engine/Registry.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_effectlist_blend.sc.bin.h"

#include <algorithm>

// Built-in names the eval blocks expose; ScanVarDecls must not treat them as
// user-declared. enabled/beat/clear/alphain/alphaout are read/write; w/h read-only.
static const std::vector<std::string> k_builtins = {
    "enabled", "beat", "clear", "alphain", "alphaout", "w", "h", "getspec", "getosc"
};

// Lua truthiness matching the JS reference: |v| > 0.1 counts as true.
static bool LuaTruthy(double v) { return v > 0.1 || v < -0.1; }


void EffectList::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_effectlist_blend_spv, sizeof(fs_effectlist_blend_spv)));
    BlendProgram = bgfx::createProgram(VS, FS, true);

    BaseTexUniform     = bgfx::createUniform("s_base",          bgfx::UniformType::Sampler);
    SrcTexUniform      = bgfx::createUniform("s_src",           bgfx::UniformType::Sampler);
    MaskTexUniform     = bgfx::createUniform("s_mask",          bgfx::UniformType::Sampler);
    BlendParamsUniform = bgfx::createUniform("u_elBlendParams", bgfx::UniformType::Vec4);

    // ── Lua evaluation override ───────────────────────────────────────────────
    for (const auto& v : k_builtins)
        m_lua.SeedVar(v);
    RescanUserVars();
    m_lua.CompileBlock(Cfg.InitCode,  "initCode",  m_initRef);
    m_lua.CompileBlock(Cfg.FrameCode, "frameCode", m_frameRef);
    m_needInit = true;
    m_inited   = true;
}

void EffectList::RescanUserVars()
{
    for (const auto& v : LuaRuntime::ScanVarDecls(Cfg.InitCode, k_builtins))
        m_lua.SeedVar(v);
}

void EffectList::OnConfigChanged(const std::vector<std::string>& Changed)
{
    if (!m_inited) return;

    for (const auto& k : Changed)
    {
        if (k == "initCode")
        {
            RescanUserVars();
            m_lua.CompileBlock(Cfg.InitCode, "initCode", m_initRef);
            m_needInit = true; // re-run Init on the next active eval frame
        }
        else if (k == "frameCode")
        {
            m_lua.CompileBlock(Cfg.FrameCode, "frameCode", m_frameRef);
        }
    }
}

void EffectList::EnsureInternalBuffers(const RenderContext& Context)
{
    const uint16_t W = (uint16_t)Context.Width;
    const uint16_t H = (uint16_t)Context.Height;
    if (InnerFbosReady &&
        InnerFbos.GetWidth()  == W &&
        InnerFbos.GetHeight() == H)
        return;

    if (InnerFbosReady)
        InnerFbos.Release();

    InnerFbos.Setup(Context.FboManager, W, H);
    InnerFbosReady = true;

    // One-time clear to black (matches original calloc of list_framebuffer).
    // Use fixed high views (252/253) that don't overlap with any chain rendering.
    auto ClearSlot = [&](FBOSlot& Slot, uint8_t View)
    {
        bgfx::setViewFrameBuffer(View, Slot.Fbo);
        bgfx::setViewClear(View, BGFX_CLEAR_COLOR, 0x000000ff);
        bgfx::setViewRect(View, 0, 0, W, H);
        bgfx::touch(View);
    };
    ClearSlot(InnerFbos.GetCurrent(), 252);
    ClearSlot(InnerFbos.GetNext(),    253);
}

void EffectList::SubmitBlend(uint8_t ViewId,
                              bgfx::TextureHandle Base,
                              bgfx::TextureHandle Src,
                              bgfx::TextureHandle Mask,
                              int32_t Mode, float Amt, bool MaskInvert,
                              bgfx::VertexBufferHandle QuadVB)
{
    const float Params[4] = { (float)Mode, Amt, MaskInvert ? 1.0f : 0.0f, 0.0f };
    bgfx::setUniform(BlendParamsUniform, Params);
    bgfx::setTexture(0, BaseTexUniform,  Base);
    bgfx::setTexture(1, SrcTexUniform,   Src);
    bgfx::setTexture(2, MaskTexUniform,  bgfx::isValid(Mask) ? Mask : Base); // dummy
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, QuadVB);
    bgfx::submit(ViewId, BlendProgram);
}

void EffectList::RenderPassThrough(const RenderContext& Context)
{
    // Pass-through: allocate one view block and blit input → output.
    const uint8_t passView = *Context.NextViewId;
    *Context.NextViewId += 4;
    bgfx::setViewFrameBuffer(passView, Context.OutputFBO);
    bgfx::setViewClear(passView, BGFX_CLEAR_NONE, 0);
    bgfx::setViewRect(passView, 0, 0, (uint16_t)Context.Width, (uint16_t)Context.Height);
    bgfx::touch(passView);
    SubmitBlend(passView,
                Context.InputTexture, Context.InputTexture, BGFX_INVALID_HANDLE,
                1, Cfg.BlendAmt, false, Context.QuadVB);
    Context.FboManager->Swap();
}

void EffectList::Render(const RenderContext& Context)
{
    // On-beat gating
    if (Context.IsBeat() && Cfg.OnBeat)
        OnBeatCooldown = Cfg.OnBeatFrames;
    const bool Active = !Cfg.OnBeat || OnBeatCooldown > 0;
    if (OnBeatCooldown > 0)
        --OnBeatCooldown;

    if (!Active)
    {
        RenderPassThrough(Context);
        return;
    }

    EnsureInternalBuffers(Context);

    const uint16_t W = (uint16_t)Context.Width;
    const uint16_t H = (uint16_t)Context.Height;

    // ── Step 0: evaluation override ───────────────────────────────────────────
    // Runs before any rendering so the script can override per-frame state.
    bool  clearThisFrame = Cfg.ClearFrame;
    float alphaIn        = Cfg.BlendAmt;
    float alphaOut       = Cfg.BlendAmt;
    if (Cfg.UseEval)
    {
        m_lua.SetAudioData(Context.AudioData);
        m_lua.SetEnvNumber("w",        (double)W);
        m_lua.SetEnvNumber("h",        (double)H);
        m_lua.SetEnvNumber("beat",     Context.IsBeat() ? 1.0 : 0.0);
        m_lua.SetEnvNumber("enabled",  1.0);
        m_lua.SetEnvNumber("clear",    clearThisFrame ? 1.0 : 0.0);
        m_lua.SetEnvNumber("alphain",  alphaIn);
        m_lua.SetEnvNumber("alphaout", alphaOut);

        if (m_needInit)
        {
            m_lua.RunBlock(m_initRef, "initCode");
            m_needInit = false;
        }
        m_lua.RunBlock(m_frameRef, "frameCode");

        // Read back the writable built-ins.
        Context.SetBeat(LuaTruthy(m_lua.GetEnvNumber("beat")));
        clearThisFrame = LuaTruthy(m_lua.GetEnvNumber("clear"));
        alphaIn  = (float)std::clamp(m_lua.GetEnvNumber("alphain"),  0.0, 1.0);
        alphaOut = (float)std::clamp(m_lua.GetEnvNumber("alphaout"), 0.0, 1.0);

        if (!LuaTruthy(m_lua.GetEnvNumber("enabled")))
        {
            RenderPassThrough(Context);
            return;
        }
    }

    // ── Step 1: optionally clear internal buffer ──────────────────────────────
    // View grabbed from the shared counter so it executes before the output blend.
    if (clearThisFrame)
    {
        const uint8_t clearView = *Context.NextViewId;
        *Context.NextViewId += 4;
        bgfx::setViewFrameBuffer(clearView, InnerFbos.GetCurrent().Fbo);
        bgfx::setViewClear(clearView, BGFX_CLEAR_COLOR, 0x000000ff);
        bgfx::setViewRect(clearView, 0, 0, W, H);
        bgfx::touch(clearView);
    }

    // ── Step 2: input blend ───────────────────────────────────────────────────
    // IGNORE (0): skip — internal buffer keeps its state.
    // Anything else: blend(internal_current, parent_input) → internal_next, then swap.
    if (Cfg.InBlend != 0)
    {
        const uint8_t blendView = *Context.NextViewId;
        *Context.NextViewId += 4;

        bgfx::TextureHandle MaskTex = BGFX_INVALID_HANDLE;
        if (Cfg.InBlend == 12)
            MaskTex = Context.FboManager->GetScratch(Cfg.InBlendBuf).Texture;

        bgfx::setViewFrameBuffer(blendView, InnerFbos.GetNext().Fbo);
        bgfx::setViewClear(blendView, BGFX_CLEAR_NONE, 0);
        bgfx::setViewRect(blendView, 0, 0, W, H);
        bgfx::touch(blendView);

        SubmitBlend(blendView,
                    InnerFbos.GetCurrent().Texture, Context.InputTexture,
                    MaskTex, Cfg.InBlend, alphaIn, Cfg.InBlendBufInvert, Context.QuadVB);
        InnerFbos.Swap();
    }

    // ── Step 3: run sub-effects ───────────────────────────────────────────────
    // Inner chain allocates from NextViewId here — these views are all lower than
    // the output blend view grabbed in Step 4, so they execute first in bgfx order.
    if (Inner.Count() > 0)
    {
        RenderContext InnerCtx   = Context;
        InnerCtx.FboManager      = &InnerFbos;
        InnerCtx.InputTexture    = InnerFbos.GetCurrent().Texture;
        InnerCtx.OutputFBO       = InnerFbos.GetNext().Fbo;
        Inner.Render(InnerCtx);
        // Each inner effect calls InnerFbos.Swap(); result ends up in GetCurrent().
    }

    // ── Step 4: output blend ──────────────────────────────────────────────────
    // Grabbed AFTER the inner chain so this view ID is higher — it executes last.
    // base=parent input, src=inner result → parent output FBO.
    const uint8_t outView = *Context.NextViewId;
    *Context.NextViewId += 4;

    bgfx::TextureHandle OutMaskTex = BGFX_INVALID_HANDLE;
    if (Cfg.OutBlend == 12)
        OutMaskTex = Context.FboManager->GetScratch(Cfg.OutBlendBuf).Texture;

    bgfx::setViewFrameBuffer(outView, Context.OutputFBO);
    bgfx::setViewClear(outView, BGFX_CLEAR_NONE, 0);
    bgfx::setViewRect(outView, 0, 0, W, H);
    bgfx::touch(outView);

    SubmitBlend(outView,
                Context.InputTexture, InnerFbos.GetCurrent().Texture,
                OutMaskTex, Cfg.OutBlend, alphaOut, Cfg.OutBlendBufInvert, Context.QuadVB);

    Context.FboManager->Swap();
}

nlohmann::json EffectList::GetConfig() const
{
    nlohmann::json j = ReflectedEffect::GetConfig();

    nlohmann::json effects = nlohmann::json::array();
    for (int32_t i = 0; i < Inner.Count(); ++i)
    {
        const EffectEntry& e = const_cast<EffectList*>(this)->Inner.GetEntry(i);
        effects.push_back({
            { "type",    e.Effect->GetDescriptor().Name },
            { "enabled", e.Enabled },
            { "config",  e.Effect->GetConfig() },
        });
    }
    j["effects"] = effects;
    return j;
}

uint8_t EffectList::ExpectedViewCount() const
{
    uint8_t n = 4; // output blend (always)
    if (!Cfg.OnBeat || OnBeatCooldown > 0)
    {
        if (Cfg.ClearFrame) n += 4;
        if (Cfg.InBlend != 0) n += 4;
        for (int32_t i = 0; i < Inner.Count(); ++i)
            n += Inner.GetEntry(i).Effect->ExpectedViewCount();
    }
    else
    {
        // Non-active pass-through uses exactly 4 views.
        n = 4;
    }
    return n;
}

void EffectList::Destroy()
{
    Inner.Clear();

    if (InnerFbosReady)
    {
        InnerFbos.Release();
        InnerFbosReady = false;
    }

    if (bgfx::isValid(BlendParamsUniform)) bgfx::destroy(BlendParamsUniform);
    if (bgfx::isValid(MaskTexUniform))    bgfx::destroy(MaskTexUniform);
    if (bgfx::isValid(SrcTexUniform))     bgfx::destroy(SrcTexUniform);
    if (bgfx::isValid(BaseTexUniform))    bgfx::destroy(BaseTexUniform);
    if (bgfx::isValid(BlendProgram))      bgfx::destroy(BlendProgram);

    BlendParamsUniform = BGFX_INVALID_HANDLE;
    MaskTexUniform     = BGFX_INVALID_HANDLE;
    SrcTexUniform      = BGFX_INVALID_HANDLE;
    BaseTexUniform     = BGFX_INVALID_HANDLE;
    BlendProgram       = BGFX_INVALID_HANDLE;
}
