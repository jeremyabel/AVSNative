#include "Engine.h"

#include "engine/AudioAnalyzer.h"

#include <algorithm>

#include "effects/AddBorders.h"
#include "effects/BufferSave.h"
#include "effects/ChannelShift.h"
#include "effects/Clear.h"
#include "effects/Grain.h"
#include "effects/Invert.h"
#include "effects/Scatter.h"
#include "effects/UniqueTone.h"
#include "effects/EffectList.h"
#include "effects/FadeOut.h"
#include "effects/Starfield.h"
#include "effects/MovingParticle.h"
#include "effects/Movement.h"
#include "effects/Simple.h"
#include "effects/Mosaic.h"
#include "effects/FastBrightness.h"
#include "effects/Mirror.h"
#include "effects/Blur.h"
#include "effects/ColorReduction.h"
#include "effects/ColorClip.h"
#include "effects/BlitEffect.h"
#include "effects/RotoBlitter.h"
#include "effects/ColorFade.h"
#include "effects/SetRenderMode.h"
#include "effects/SuperScope.h"
#include "effects/Interferences.h"
#include "effects/Interleave.h"
#include "effects/MultiFilter.h"
#include "effects/Multiplier.h"
#include "effects/OnBeatClear.h"
#include "effects/Bump.h"
#include "effects/Water.h"
#include "effects/WaterBump.h"
#include "effects/DotGrid.h"
#include "effects/BassSpin.h"
#include "effects/Normalize.h"
#include "effects/ColorModifier.h"
#include "effects/DynamicDistanceModifier.h"
#include "effects/DynamicShift.h"
#include "effects/Texer.h"
#include "effects/Texer2.h"
#include "effects/RotatingStars.h"
#include "effects/OscilloscopeStar.h"
#include "effects/Ring.h"
#include "effects/Picture.h"
#include "effects/Picture2.h"
#include "effects/ImageGrid.h"
#include "effects/Convolution.h"
#include "effects/ColorMap.h"
#include "effects/CustomBpm.h"
#include "effects/MultiDelay.h"
#include "effects/Triangle.h"
#include "effects/VideoDelay.h"
#include "effects/DynamicMovement.h"
#include "effects/DotFountain.h"
#include "effects/DotPlane.h"
#include "effects/Timescope.h"
#include "effects/Brightness.h"
#include "effects/Text.h"
#include "ShaderCompiler.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_blit.sc.bin.h"

static const float k_quadVerts[] = {
    -1.0f, -1.0f,
    -1.0f,  3.0f,
     3.0f, -1.0f,
};

bool Engine::Init(const EngineConfig& Config, bgfx::RendererType::Enum Renderer)
{
    Width = Config.Width;
    Height = Config.Height;
    RendererType = Renderer;

    ShaderCompiler::Init();
    FboManager.Init((uint16_t)Width, (uint16_t)Height);
    InitBlit();
    Audio.Init();
    InitAudioTex();

    EffectRegistry.Register("Add Borders",    []() { return std::make_unique<AddBorders>(); });
    EffectRegistry.Register("Buffer Save",    []() { return std::make_unique<BufferSave>(); });
    EffectRegistry.Register("Channel Shift",  []() { return std::make_unique<ChannelShift>(); });
    EffectRegistry.Register("Clear",          []() { return std::make_unique<Clear>(); });
    EffectRegistry.Register("Grain",          []() { return std::make_unique<Grain>(); });
    EffectRegistry.Register("Invert",         []() { return std::make_unique<Invert>(); });
    EffectRegistry.Register("Scatter",        []() { return std::make_unique<Scatter>(); });
    EffectRegistry.Register("Unique Tone",    []() { return std::make_unique<UniqueTone>(); });
    EffectRegistry.Register("Effect List",    []() { return std::make_unique<EffectList>(); });
    EffectRegistry.Register("FadeOut",        []() { return std::make_unique<FadeOut>(); });
    EffectRegistry.Register("MovingParticle", []() { return std::make_unique<MovingParticle>(); });
    EffectRegistry.Register("Movement",       []() { return std::make_unique<Movement>(); });
    EffectRegistry.Register("Starfield",      []() { return std::make_unique<Starfield>(); });
    EffectRegistry.Register("Simple",         []() { return std::make_unique<Simple>(); });
    EffectRegistry.Register("Mosaic",         []() { return std::make_unique<Mosaic>(); });
    EffectRegistry.Register("Fast Brightness",[]() { return std::make_unique<FastBrightness>(); });
    EffectRegistry.Register("Mirror",         []() { return std::make_unique<Mirror>(); });
    EffectRegistry.Register("Blur",           []() { return std::make_unique<Blur>(); });
    EffectRegistry.Register("Color Reduction",[]() { return std::make_unique<ColorReduction>(); });
    EffectRegistry.Register("Color Clip",     []() { return std::make_unique<ColorClip>(); });
    EffectRegistry.Register("Blit",           []() { return std::make_unique<BlitEffect>(); });
    EffectRegistry.Register("Roto Blitter",   []() { return std::make_unique<RotoBlitter>(); });
    EffectRegistry.Register("Colorfade",        []() { return std::make_unique<ColorFade>(); });
    EffectRegistry.Register("Set Render Mode",  []() { return std::make_unique<SetRenderMode>(); });
    EffectRegistry.Register("Super Scope",      []() { return std::make_unique<SuperScope>(); });
    EffectRegistry.Register("Interferences",    []() { return std::make_unique<Interferences>(); });
    EffectRegistry.Register("Interleave",       []() { return std::make_unique<Interleave>(); });
    EffectRegistry.Register("Multi Filter",     []() { return std::make_unique<MultiFilter>(); });
    EffectRegistry.Register("Multiplier",       []() { return std::make_unique<Multiplier>(); });
    EffectRegistry.Register("OnBeat Clear",     []() { return std::make_unique<OnBeatClear>(); });
    EffectRegistry.Register("Bump",             []() { return std::make_unique<Bump>(); });
    EffectRegistry.Register("Water",            []() { return std::make_unique<Water>(); });
    EffectRegistry.Register("Water Bump",       []() { return std::make_unique<WaterBump>(); });
    EffectRegistry.Register("Dot Grid",         []() { return std::make_unique<DotGrid>(); });
    EffectRegistry.Register("Bass Spin",        []() { return std::make_unique<BassSpin>(); });
    EffectRegistry.Register("Normalize",        []() { return std::make_unique<Normalize>(); });
    EffectRegistry.Register("Color Modifier",   []() { return std::make_unique<ColorModifier>(); });
    EffectRegistry.Register("Dynamic Distance Modifier", []() { return std::make_unique<DynamicDistanceModifier>(); });
    EffectRegistry.Register("Rotating Stars",     []() { return std::make_unique<RotatingStars>(); });
    EffectRegistry.Register("Oscilloscope Star", []() { return std::make_unique<OscilloscopeStar>(); });
    EffectRegistry.Register("Ring",              []() { return std::make_unique<Ring>(); });
    EffectRegistry.Register("Picture",            []() { return std::make_unique<Picture>(); });
    EffectRegistry.Register("Picture II",         []() { return std::make_unique<Picture2>(); });
    EffectRegistry.Register("Convolution Filter", []() { return std::make_unique<Convolution>(); });
    EffectRegistry.Register("Color Map",          []() { return std::make_unique<ColorMap>(); });
    EffectRegistry.Register("Custom BPM",         []() { return std::make_unique<CustomBpm>(); });
    EffectRegistry.Register("Dynamic Shift",      []() { return std::make_unique<DynamicShift>(); });
    EffectRegistry.Register("Texer",              []() { return std::make_unique<Texer>(); });
    EffectRegistry.Register("Texer II",           []() { return std::make_unique<Texer2>(); });
    EffectRegistry.Register("Image Grid",         []() { return std::make_unique<ImageGrid>(); });
    EffectRegistry.Register("Multi Delay",        []() { return std::make_unique<MultiDelay>(); });
    EffectRegistry.Register("Triangle",           []() { return std::make_unique<Triangle>(); });
    EffectRegistry.Register("Video Delay",        []() { return std::make_unique<VideoDelay>(); });
    EffectRegistry.Register("Dynamic Movement",   []() { return std::make_unique<DynamicMovement>(); });
    EffectRegistry.Register("Dot Fountain",       []() { return std::make_unique<DotFountain>(); });
    EffectRegistry.Register("Dot Plane",          []() { return std::make_unique<DotPlane>(); });
    EffectRegistry.Register("Timescope",          []() { return std::make_unique<Timescope>(); });
    EffectRegistry.Register("Brightness",         []() { return std::make_unique<Brightness>(); });
    EffectRegistry.Register("Text",               []() { return std::make_unique<Text>(); });

    // Clear the initial ping-pong FBO to black. Without this the first frame
    // reads uninitialized texture memory as the effect chain input.
    bgfx::setViewFrameBuffer(0, FboManager.GetCurrent().Fbo);
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR, 0x000000ff);
    bgfx::setViewRect(0, 0, 0, (uint16_t)Width, (uint16_t)Height);
    bgfx::touch(0);
    bgfx::frame();

    Initialized = true;
    return true;
}

void Engine::Shutdown()
{
    if (!Initialized)
        return;

    Chain.Clear();    // Destroy() each effect's bgfx handles before bgfx::shutdown
    DestroyBlit();
    DestroyAudioTex();
    FboManager.Destroy();
    Audio.Shutdown();
    ShaderCompiler::Shutdown();
    Initialized = false;
}

void Engine::Tick()
{
    if (!Initialized)
        return;

    Frame++;

    Audio.Update();
    UploadAudioTex();

    uint8_t viewCounter = 0;
    uint32_t lineBlendMode = (1u << 16); // default: lineWidth=1, alpha=0, Replace
    // Per-frame beat, shared by pointer so Custom BPM can rewrite it for downstream effects.
    bool beat = Audio.IsBeat();

    RenderContext Context{};
    Context.FboManager = &FboManager;
    Context.QuadVB = BlitQuadVB;
    Context.NextViewId = &viewCounter;
    Context.LineBlendMode = &lineBlendMode;
    Context.IsBeatPtr = &beat;
    Context.AudioTex = AudioTex;
    Context.AudioData = &Audio.GetVisData();
    Context.Width = Width;
    Context.Height = Height;
    Context.Frame = Frame;
    Context.Time = Time;

    Chain.Render(Context);

    // Blit final FBO to the output framebuffer on view 254.
    bgfx::setViewRect(254, 0, 0, (uint16_t)Width, (uint16_t)Height);
    bgfx::setViewFrameBuffer(254, OutputFbo);
    SubmitBlit(254);
}

void Engine::Resize(int Width, int Height)
{
    this->Width = Width;
    this->Height = Height;
    FboManager.Resize((uint16_t)Width, (uint16_t)Height);
    // bgfx::reset() is NOT called here; App owns the primary window swapchain.
}

void Engine::SetOutputFrameBuffer(bgfx::FrameBufferHandle Fbo)
{
    OutputFbo = Fbo;
}

EffectChain& Engine::GetChain()
{
    return Chain;
}

FBOManager& Engine::GetFBOManager()
{
    return FboManager;
}

Registry& Engine::GetRegistry()
{
    return EffectRegistry;
}

AudioAnalyzer& Engine::GetAudio()
{
    return Audio;
}

bgfx::RendererType::Enum Engine::GetRendererType() const
{
    return RendererType;
}

int Engine::GetWidth() const
{
    return Width;
}

int Engine::GetHeight() const
{
    return Height;
}

void Engine::InitBlit()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_blit_spv, sizeof(fs_blit_spv)));
    BlitProgram = bgfx::createProgram(VS, FS, true);

    BlitTexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

    bgfx::VertexLayout Layout;
    Layout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .end();

    BlitQuadVB = bgfx::createVertexBuffer(bgfx::copy(k_quadVerts, sizeof(k_quadVerts)), Layout);
}

void Engine::DestroyBlit()
{
    if (bgfx::isValid(BlitQuadVB))    
        bgfx::destroy(BlitQuadVB);

    if (bgfx::isValid(BlitTexUniform))
        bgfx::destroy(BlitTexUniform);

    if (bgfx::isValid(BlitProgram))   
        bgfx::destroy(BlitProgram);

    BlitQuadVB = BGFX_INVALID_HANDLE;
    BlitTexUniform = BGFX_INVALID_HANDLE;
    BlitProgram = BGFX_INVALID_HANDLE;
}

void Engine::SubmitBlit(uint8_t ViewId)
{
    bgfx::setTexture(0, BlitTexUniform, FboManager.GetCurrent().Texture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, BlitQuadVB);
    bgfx::submit(ViewId, BlitProgram);
}

void Engine::InitAudioTex()
{
    // 576×1 RGBA8, full mip chain (10 levels for width 576).
    // Sampler flags: point filtering, clamp — texelFetch ignores the sampler,
    // but conventional sampling from dynamic effects should use nearest.
    const uint64_t flags = BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_U_CLAMP   | BGFX_SAMPLER_V_CLAMP;
    AudioTex = bgfx::createTexture2D(
        kAudioBins, 1, /*hasMips=*/true, /*numLayers=*/1,
        bgfx::TextureFormat::RGBA8, flags);

    // Upload zero-filled mip 0 immediately so the texture is valid before any audio arrives.
    static const uint8_t zeros[kAudioBins * 4] = {};
    bgfx::updateTexture2D(AudioTex, 0, 0, 0, 0, kAudioBins, 1, bgfx::copy(zeros, sizeof(zeros)));
}

void Engine::DestroyAudioTex()
{
    if (bgfx::isValid(AudioTex))
    {
        bgfx::destroy(AudioTex);
        AudioTex = BGFX_INVALID_HANDLE;
    }
}

void Engine::UploadAudioTex()
{
    if (!bgfx::isValid(AudioTex)) 
    {
        return;
    }

    const VisData& vd = Audio.GetVisData();

    // Build mip 0 (576 RGBA8 pixels): R=specL, G=specR, B=oscL, A=oscR.
    uint8_t mip0[kAudioBins * 4];
    for (int i = 0; i < kAudioBins; i++)
    {
        mip0[i * 4 + 0] = (uint8_t)std::clamp((int)vd.spec[0][i], 0, 255);
        mip0[i * 4 + 1] = (uint8_t)std::clamp((int)vd.spec[1][i], 0, 255);
        mip0[i * 4 + 2] = (uint8_t)std::clamp((int)vd.osc [0][i], 0, 255);
        mip0[i * 4 + 3] = (uint8_t)std::clamp((int)vd.osc [1][i], 0, 255);
    }

    // Upload all mip levels. Each level is a box-filtered (averaged pairs) downsample
    // of the previous, computed in-place on a single scratch buffer.
    // In-place downsampling is safe: output at index i reads from 2i and 2i+1
    // which are always ahead of (or equal to, for i=0) any previous write.
    uint8_t buf[kAudioBins * 4];
    const uint8_t* src = mip0;
    int w = kAudioBins;

    for (int mip = 0; ; mip++)
    {
        bgfx::updateTexture2D(AudioTex, 0, (uint8_t)mip, 0, 0, (uint16_t)w, 1, bgfx::copy(src, (uint32_t)(w * 4)));

        if (w <= 1) 
        {
            break;
        }

        int dw = std::max(1, w / 2);
        for (int i = 0; i < dw; i++)
        {
            for (int c = 0; c < 4; c++)
            {
                int a = src[i * 2 * 4 + c];
                int b = (i * 2 + 1 < w) ? src[(i * 2 + 1) * 4 + c] : a;
                buf[i * 4 + c] = (uint8_t)((a + b) / 2);
            }
        }

        // After the first iteration src=mip0→dst=buf; subsequently buf→buf in-place.
        src = buf;
        w = dw;
    }
}
