#pragma once

#include "AudioAnalyzer.h"
#include "EffectChain.h"
#include "FBOManager.h"
#include "GlobalSlider.h"
#include "Registry.h"
#include <bgfx/bgfx.h>
#include <chrono>
#include <vector>

struct EngineConfig
{
    int32_t Width = 1280;
    int32_t Height = 720;
};

class Engine
{
public:

    bool Init(const EngineConfig& Config, bgfx::RendererType::Enum Renderer);
    void Shutdown();

    // Runs one frame: audio update (future), effect chain, blit to output FBO.
    // Does NOT call bgfx::frame() — the caller does that.
    void Tick();

    // Resize the internal render FBOs when the output window changes size.
    // Does NOT call bgfx::reset() — the caller handles that for the primary window.
    void Resize(int32_t Width, int32_t Height);

    // Direct view 254 (the final blit) to a specific framebuffer.
    // Pass BGFX_INVALID_HANDLE to blit to the bgfx backbuffer (primary window).
    void SetOutputFrameBuffer(bgfx::FrameBufferHandle Fbo);

    EffectChain& GetChain();
    FBOManager& GetFBOManager();
    Registry& GetRegistry();
    AudioAnalyzer& GetAudio();

    // Preset-global named sliders (read from Lua via slider("name")), edited in the
    // Sliders panel and serialized with the preset.
    std::vector<GlobalSlider>& GetSliders() { return Sliders; }

    bgfx::RendererType::Enum GetRendererType() const;
    int32_t GetWidth() const;
    int32_t GetHeight() const;

private:

    void InitBlit();
    void DestroyBlit();
    void SubmitBlit(uint8_t ViewId);

    EffectChain Chain;
    std::vector<GlobalSlider> Sliders;
    FBOManager FboManager;
    Registry EffectRegistry;
    AudioAnalyzer Audio;
    bgfx::RendererType::Enum RendererType = bgfx::RendererType::Count;
    int32_t Width = 0;
    int32_t Height = 0;
    int32_t Frame = 0;
    double Time = 0.0;
    bool Initialized = false;

    // Wall-clock delta-time tracking (drives the Lua `dt` variable).
    std::chrono::steady_clock::time_point LastTick;
    bool HaveTick = false;

    bgfx::FrameBufferHandle OutputFbo = BGFX_INVALID_HANDLE; // set by App for the output window

    bgfx::ProgramHandle BlitProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle BlitTexUniform = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle BlitQuadVB = BGFX_INVALID_HANDLE;

    // 576×1 RGBA8 texture with full mip chain for shader audio access.
    // Layout: R=spec_L, G=spec_R, B=osc_L, A=osc_R (0–255 each).
    bgfx::TextureHandle AudioTex = BGFX_INVALID_HANDLE;

    void InitAudioTex();
    void DestroyAudioTex();
    void UploadAudioTex();
};
