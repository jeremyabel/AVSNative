#include "ImageGrid.h"

#include "engine/FBOManager.h"
#include "thirdparty/StbGifStream.h"

#include <stb/stb_image.h>

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_imagegrid.sc.bin.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

// ── Engine-set built-ins, excluded from the user-var scan ─────────────────────
// x,y,sizex,sizey,r persist across frames (set in Init, mutated in Frame/Beat);
// width,height,b are written every frame. All are seeded separately, so they must
// not be reported as user vars by ScanVarDecls.
const std::vector<std::string> ImageGrid::k_builtins = {
    "x", "y", "sizex", "sizey", "r", "width", "height", "b",
};

// ── $pi substitution ─────────────────────────────────────────────────────────

static std::string SubstitutePi(std::string code)
{
    size_t pos = 0;
    while ((pos = code.find("$pi", pos)) != std::string::npos) {
        code.replace(pos, 3, "pi");
        pos += 2;
    }
    return code;
}

// ── GetConfig / SetConfig ─────────────────────────────────────────────────────

nlohmann::json ImageGrid::GetConfig() const
{
    nlohmann::json j = ReflectedEffect<ImageGridConfig>::GetConfig();
    j["imageData"]   = Cfg.ImageData;
    j["imageData2"]  = Cfg.ImageData2;
    j["activeImage"] = Cfg.ActiveImage;
    return j;
}

void ImageGrid::SetConfig(const nlohmann::json& cfg)
{
    ReflectedEffect<ImageGridConfig>::SetConfig(cfg);

    bool activeChanged = false;

    if (cfg.contains("activeImage") && cfg["activeImage"].is_number_integer())
    {
        const int v = cfg["activeImage"].get<int>() ? 1 : 0;
        if (v != Cfg.ActiveImage) { Cfg.ActiveImage = v; activeChanged = true; }
    }
    // imageData/imageData2 are bundle asset references — the raw bytes arrive via
    // ApplyAsset, not decoded here. Just store the strings for round-trip.
    if (cfg.contains("imageData") && cfg["imageData"].is_string())
        Cfg.ImageData = cfg["imageData"].get<std::string>();
    if (cfg.contains("imageData2") && cfg["imageData2"].is_string())
        Cfg.ImageData2 = cfg["imageData2"].get<std::string>();

    // On a slot toggle, rebuild the texture from the (already cached) raw bytes.
    if (m_inited && activeChanged)
        LoadActiveImage();
}

// ── Preset bundle assets ──────────────────────────────────────────────────────

std::vector<PresetAsset> ImageGrid::CollectAssets() const
{
    std::vector<PresetAsset> out;
    if (!m_slotRaw[0].empty())
        out.push_back({ "imageData",  m_slotName[0], m_slotRaw[0] });
    if (!m_slotRaw[1].empty())
        out.push_back({ "imageData2", m_slotName[1], m_slotRaw[1] });
    return out;
}

void ImageGrid::ApplyAsset(const std::string& key, const std::string& name,
                           std::vector<uint8_t> bytes)
{
    const int slot = (key == "imageData2") ? 1 : 0;
    m_slotRaw[slot]  = std::move(bytes);
    m_slotName[slot] = name;
    (slot == 1 ? Cfg.ImageData2 : Cfg.ImageData) = name;  // non-empty marker for UI/round-trip

    if (m_inited && slot == (Cfg.ActiveImage == 1 ? 1 : 0))
        LoadActiveImage();
}

// ── OnConfigChanged ───────────────────────────────────────────────────────────

void ImageGrid::OnConfigChanged(const std::vector<std::string>& changed)
{
    if (!m_inited) return;

    bool recompileInit = false, recompileFrame = false, recompileBeat = false;
    for (const auto& key : changed)
    {
        if (key == "initCode")  recompileInit = true;
        if (key == "frameCode") recompileFrame = true;
        if (key == "beatCode")  recompileBeat = true;
    }

    if (recompileInit)
    {
        // An init-code change may add/remove user vars: re-scan + recompile everything.
        RescanAndSeed();
        CompileAll();
        RunInit();
    }
    else
    {
        if (recompileFrame) m_lua.CompileBlock(SubstitutePi(Cfg.FrameCode), "frameCode", m_frameRef);
        if (recompileBeat)  m_lua.CompileBlock(SubstitutePi(Cfg.BeatCode),  "beatCode",  m_beatRef);
    }
}

// ── Default 64×64 checkerboard ────────────────────────────────────────────────

void ImageGrid::MakeDefaultImage()
{
    ResetAnimation();
    const int sz = 64, cell = 8;
    m_imgW = m_imgH = sz;
    std::vector<uint8_t> px(sz * sz * 4);
    for (int y = 0; y < sz; y++)
        for (int x = 0; x < sz; x++)
        {
            const bool on = (((x / cell) + (y / cell)) & 1) != 0;
            const uint8_t v = on ? 200 : 60;
            const int i = (y * sz + x) * 4;
            px[i] = px[i+1] = px[i+2] = v;
            px[i+3] = 255;
        }

    if (bgfx::isValid(m_imageTex)) { bgfx::destroy(m_imageTex); m_imageTex = BGFX_INVALID_HANDLE; }
    // Default sampler flags (0) = repeat wrap + bilinear, which is what tiling wants.
    m_imageTex = bgfx::createTexture2D((uint16_t)sz, (uint16_t)sz, false, 1,
        bgfx::TextureFormat::RGBA8, 0,
        bgfx::copy(px.data(), (uint32_t)px.size()));
}

// ── Animation state ───────────────────────────────────────────────────────────

void ImageGrid::ResetAnimation()
{
    if (m_gif) { GifStreamClose(m_gif); m_gif = nullptr; }
    m_frames.clear();
    m_frameDelaysMs.clear();
    m_fullyCached  = false;
    m_curFrame     = 0;
    m_uploadedFrame = (size_t)-1;
    m_frameAccumMs = 0.0;
    m_haveTick     = false;
}

// Upload the current frame's cached pixels into the mutable texture, but only
// when the displayed frame actually changed (the sole per-frame GPU cost).
void ImageGrid::UploadCurrentFrame()
{
    if (m_curFrame >= m_frames.size() || m_curFrame == m_uploadedFrame) return;
    if (!bgfx::isValid(m_imageTex)) return;

    const uint32_t frameBytes = (uint32_t)((size_t)m_imgW * m_imgH * 4);
    bgfx::updateTexture2D(m_imageTex, 0, 0, 0, 0, (uint16_t)m_imgW, (uint16_t)m_imgH,
        bgfx::copy(m_frames[m_curFrame].data(), frameBytes));
    m_uploadedFrame = m_curFrame;
}

// Advance m_curFrame by wall-clock time, decoding + caching frames on demand the
// first time through, then uploading the current frame to the mutable texture.
void ImageGrid::AdvanceAnimation()
{
    if (m_frames.empty()) return;                       // static image
    if (m_fullyCached && m_frames.size() <= 1) return;  // single-frame GIF

    const auto now = std::chrono::steady_clock::now();
    if (!m_haveTick) { m_lastTick = now; m_haveTick = true; return; }

    const double elapsedMs = std::chrono::duration<double, std::milli>(now - m_lastTick).count();
    m_lastTick = now;

    // Clamp huge gaps (window unfocused, breakpoint, …) so the catch-up loop can't spin.
    m_frameAccumMs += std::min(elapsedMs, 1000.0);

    size_t guard = 0;
    while (m_frameAccumMs >= (double)m_frameDelaysMs[m_curFrame] && guard++ < 240)
    {
        m_frameAccumMs -= (double)m_frameDelaysMs[m_curFrame];

        size_t next = m_curFrame + 1;
        if (next < m_frames.size())
        {
            // Already decoded earlier — replay from cache.
        }
        else if (m_gif)
        {
            // First time reaching this frame: decode + cache it.
            const uint8_t* px = nullptr; int delay = 0;
            if (GifStreamNextFrame(m_gif, &px, &delay))
            {
                const size_t frameBytes = (size_t)m_imgW * m_imgH * 4;
                m_frames.emplace_back(px, px + frameBytes);
                m_frameDelaysMs.push_back(delay > 0 ? delay : 100);
            }
            else
            {
                // End of stream: the cache now holds every frame. Stop decoding and
                // loop back to frame 0 (subsequent loops replay from the cache).
                GifStreamClose(m_gif); m_gif = nullptr;
                m_fullyCached = true;
                next = 0;
            }
        }
        else
        {
            next %= m_frames.size();  // fully cached — wrap around
        }

        if (m_frames.size() <= 1) { m_curFrame = 0; break; }  // single-frame GIF
        m_curFrame = next % m_frames.size();
    }

    UploadCurrentFrame();
}

// ── Image loading (raw bytes cached per slot, delivered via ApplyAsset) ───────

// (Re)build the texture from the active slot's cached raw bytes. Only frame 0 of a
// GIF is decoded here; the rest stream in during playback (AdvanceAnimation).
void ImageGrid::LoadActiveImage()
{
    ResetAnimation();

    const std::vector<uint8_t>& raw = m_slotRaw[Cfg.ActiveImage == 1 ? 1 : 0];
    if (raw.empty()) { MakeDefaultImage(); return; }

    // Try the streaming GIF decoder first — returns null for any non-GIF format.
    int gw = 0, gh = 0;
    if (GifStream* gs = GifStreamOpen(raw.data(), (int)raw.size(), &gw, &gh))
    {
        const uint8_t* px0 = nullptr; int delay0 = 0;
        if (GifStreamNextFrame(gs, &px0, &delay0))
        {
            m_imgW = gw; m_imgH = gh;
            const uint32_t frameBytes = (uint32_t)((size_t)gw * gh * 4);

            m_frames.emplace_back(px0, px0 + frameBytes);
            m_frameDelaysMs.push_back(delay0 > 0 ? delay0 : 100);

            if (bgfx::isValid(m_imageTex)) { bgfx::destroy(m_imageTex); m_imageTex = BGFX_INVALID_HANDLE; }

            // Mutable texture (no initial memory) so AdvanceAnimation can update it per
            // frame. Flags 0 = repeat wrap + bilinear, same as the static path.
            m_imageTex = bgfx::createTexture2D((uint16_t)gw, (uint16_t)gh, false, 1,
                bgfx::TextureFormat::RGBA8, 0);
            bgfx::updateTexture2D(m_imageTex, 0, 0, 0, 0, (uint16_t)gw, (uint16_t)gh,
                bgfx::copy(m_frames[0].data(), frameBytes));
            m_uploadedFrame = 0;

            m_gif = gs;  // keep open to decode remaining frames lazily
            return;
        }
        GifStreamClose(gs);  // 0-frame / corrupt GIF — fall through to static decode
    }

    // Non-GIF (PNG/JPG/BMP/TGA/…): decode the single image.
    int sw = 0, sh = 0, sch = 0;
    uint8_t* pixels = stbi_load_from_memory(raw.data(), (int)raw.size(), &sw, &sh, &sch, 4);
    if (!pixels) { MakeDefaultImage(); return; }

    if (bgfx::isValid(m_imageTex)) { bgfx::destroy(m_imageTex); m_imageTex = BGFX_INVALID_HANDLE; }

    m_imgW = sw; m_imgH = sh;
    const bgfx::Memory* mem = bgfx::copy(pixels, (uint32_t)(sw * sh * 4));
    stbi_image_free(pixels);

    // Repeat wrap (default flags) so fract() tiling has no edge seams.
    m_imageTex = bgfx::createTexture2D((uint16_t)sw, (uint16_t)sh, false, 1,
        bgfx::TextureFormat::RGBA8, 0, mem);
}

// ── Lua helpers ───────────────────────────────────────────────────────────────

void ImageGrid::RescanAndSeed()
{
    for (const auto& name : k_builtins)
        m_lua.SeedVar(name);

    const std::string allCode =
        Cfg.InitCode + "\n" + Cfg.FrameCode + "\n" + Cfg.BeatCode;
    for (const auto& name : LuaRuntime::ScanVarDecls(allCode, k_builtins))
        m_lua.SeedVar(name);
}

void ImageGrid::CompileAll()
{
    m_lua.CompileBlock(SubstitutePi(Cfg.InitCode),  "initCode",  m_initRef);
    m_lua.CompileBlock(SubstitutePi(Cfg.FrameCode), "frameCode", m_frameRef);
    m_lua.CompileBlock(SubstitutePi(Cfg.BeatCode),  "beatCode",  m_beatRef);
}

void ImageGrid::RunInit()
{
    m_lua.RunBlock(m_initRef, "initCode");
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

// Extra Lua built-ins not in LuaRuntime's default math aliases (matches Texer2).
static const char k_setupCode[] = R"(
rand  = function(n) return math.floor(math.random() * n) end
above = function(a,b) return (a > b) and 1 or 0 end
below = function(a,b) return (a < b) and 1 or 0 end
equal = function(a,b) return (a == b) and 1 or 0 end
sign  = function(n) return (n > 0) and 1 or ((n < 0) and -1 or 0) end
int   = function(n) return n >= 0 and math.floor(n) or math.ceil(n) end
atan  = function(a,b) if b ~= nil then return math.atan2(a,b) else return math.atan(a) end end
)";

void ImageGrid::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_imagegrid_spv, sizeof(fs_imagegrid_spv)));
    m_prog = bgfx::createProgram(VS, FS, true);

    m_inputUnif  = bgfx::createUniform("s_input",      bgfx::UniformType::Sampler);
    m_imageUnif  = bgfx::createUniform("s_image",      bgfx::UniformType::Sampler);
    m_xformUnif  = bgfx::createUniform("u_gridXform",  bgfx::UniformType::Vec4);
    m_paramsUnif = bgfx::createUniform("u_gridParams", bgfx::UniformType::Vec4);

    // Seed extra Lua built-ins (rand, above, below, equal, sign, int, atan 2-arg).
    int setupRef = -1;
    m_lua.CompileBlock(k_setupCode, "setup", setupRef);
    m_lua.RunBlock(setupRef, "setup");

    RescanAndSeed();
    CompileAll();

    m_inited = true;
    LoadActiveImage();  // shows the default checkerboard until ApplyAsset delivers bytes
    RunInit();
}

void ImageGrid::Destroy()
{
    if (m_gif) { GifStreamClose(m_gif); m_gif = nullptr; }
    m_frames.clear();
    m_frameDelaysMs.clear();
    m_slotRaw[0].clear();
    m_slotRaw[1].clear();
    m_slotName[0].clear();
    m_slotName[1].clear();
    if (bgfx::isValid(m_imageTex))   bgfx::destroy(m_imageTex);
    if (bgfx::isValid(m_paramsUnif)) bgfx::destroy(m_paramsUnif);
    if (bgfx::isValid(m_xformUnif))  bgfx::destroy(m_xformUnif);
    if (bgfx::isValid(m_imageUnif))  bgfx::destroy(m_imageUnif);
    if (bgfx::isValid(m_inputUnif))  bgfx::destroy(m_inputUnif);
    if (bgfx::isValid(m_prog))       bgfx::destroy(m_prog);

    m_imageTex   = BGFX_INVALID_HANDLE;
    m_paramsUnif = BGFX_INVALID_HANDLE;
    m_xformUnif  = BGFX_INVALID_HANDLE;
    m_imageUnif  = BGFX_INVALID_HANDLE;
    m_inputUnif  = BGFX_INVALID_HANDLE;
    m_prog       = BGFX_INVALID_HANDLE;
    m_imgW = m_imgH = 0;
    m_inited = false;
}

// ── Render ────────────────────────────────────────────────────────────────────

void ImageGrid::Render(const RenderContext& Ctx)
{
    AdvanceAnimation();

    const int w = Ctx.Width, h = Ctx.Height;

    // Run per-frame Lua. width/height/b are engine-set; x/y/sizex/sizey/r persist.
    m_lua.SetAudioData(Ctx.AudioData);
    m_lua.SetEnvNumber("width",  (double)w);
    m_lua.SetEnvNumber("height", (double)h);
    m_lua.SetEnvNumber("b",      Ctx.IsBeat() ? 1.0 : 0.0);

    m_lua.RunBlock(m_frameRef, "frameCode");
    if (Ctx.IsBeat())
        m_lua.RunBlock(m_beatRef, "beatCode");

    const float x     = (float)m_lua.GetEnvNumber("x");
    const float y     = (float)m_lua.GetEnvNumber("y");
    const float sizex = (float)m_lua.GetEnvNumber("sizex");
    const float sizey = (float)m_lua.GetEnvNumber("sizey");
    const float r     = (float)m_lua.GetEnvNumber("r");

    const float imgAspect = (m_imgH > 0) ? (float)m_imgW / (float)m_imgH : 1.0f;
    const float scrAspect = (h > 0) ? (float)w / (float)h : 1.0f;

    float xform[4]  = { x, y, sizex, sizey };
    float params[4] = { r, imgAspect, scrAspect, (float)Cfg.BlendMode };
    bgfx::setUniform(m_xformUnif,  xform);
    bgfx::setUniform(m_paramsUnif, params);

    bgfx::setTexture(0, m_inputUnif, Ctx.InputTexture);
    bgfx::setTexture(1, m_imageUnif, m_imageTex);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Ctx.QuadVB);
    bgfx::submit(Ctx.ViewId, m_prog);

    Ctx.FboManager->Swap();
}
