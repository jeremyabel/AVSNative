#include "imgui_impl_bgfx.h"

#include <bgfx/embedded_shader.h>
#include <bx/math.h>

// Compiled ImGui shaders shipped with bgfx's examples (renderer-agnostic blobs).
// We only reuse the data headers; none of the example C++ backend is compiled.
#include "vs_ocornut_imgui.bin.h"
#include "fs_ocornut_imgui.bin.h"

static const bgfx::EmbeddedShader s_embeddedShaders[] =
{
    BGFX_EMBEDDED_SHADER(vs_ocornut_imgui),
    BGFX_EMBEDDED_SHADER(fs_ocornut_imgui),
    BGFX_EMBEDDED_SHADER_END()
};

// ── Backend state ─────────────────────────────────────────────────────────────

struct ImplBgfxData
{
    bgfx::VertexLayout  Layout;
    bgfx::ProgramHandle Program  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexSampler = BGFX_INVALID_HANDLE;
    bgfx::ViewId        ViewId   = 255;
    bool                Initialized = false;
};

static ImplBgfxData g_bd;

// ── ImTextureID ↔ bgfx handle packing ─────────────────────────────────────────
// ImTextureID_Invalid is 0 and a valid bgfx handle can have idx 0, so set a high
// sentinel bit to guarantee a created texture maps to a non-zero ImTextureID.

static constexpr ImTextureID kTexSentinel = (ImTextureID)1 << 48;

static inline ImTextureID ToImTex(bgfx::TextureHandle h)
{
    return (ImTextureID)h.idx | kTexSentinel;
}
static inline bgfx::TextureHandle FromImTex(ImTextureID id)
{
    return bgfx::TextureHandle{ (uint16_t)(id & 0xFFFF) };
}

// ── Texture management (1.92 ImTextureData API) ───────────────────────────────

static void UpdateTexture(ImTextureData* tex)
{
    if (tex->Status == ImTextureStatus_WantCreate)
    {
        // ImGui atlas is RGBA32; upload as BGRA8. For the font atlas RGB are white,
        // so the channel order is irrelevant; UI geometry is colored per-vertex.
        bgfx::TextureHandle handle = bgfx::createTexture2D(
            (uint16_t)tex->Width, (uint16_t)tex->Height, false, 1,
            bgfx::TextureFormat::BGRA8, 0);
        bgfx::setName(handle, "ImGui Atlas");
        bgfx::updateTexture2D(handle, 0, 0, 0, 0,
            (uint16_t)tex->Width, (uint16_t)tex->Height,
            bgfx::copy(tex->GetPixels(), tex->GetSizeInBytes()));

        tex->SetTexID(ToImTex(handle));
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantUpdates)
    {
        bgfx::TextureHandle handle = FromImTex(tex->GetTexID());
        const uint32_t bpp = (uint32_t)tex->BytesPerPixel;
        for (ImTextureRect& r : tex->Updates)
        {
            const bgfx::Memory* mem = bgfx::alloc((uint32_t)r.w * r.h * bpp);
            bx::gather(mem->data, tex->GetPixelsAt(r.x, r.y), tex->GetPitch(),
                       (uint32_t)r.w * bpp, r.h);
            bgfx::updateTexture2D(handle, 0, 0, r.x, r.y, r.w, r.h, mem);
        }
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantDestroy)
    {
        bgfx::TextureHandle handle = FromImTex(tex->GetTexID());
        if (bgfx::isValid(handle))
            bgfx::destroy(handle);
        tex->SetTexID(ImTextureID_Invalid);
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
}

// ── Transient buffer availability ─────────────────────────────────────────────

static bool CheckAvail(uint32_t numVtx, uint32_t numIdx, bool idx32)
{
    return numVtx == bgfx::getAvailTransientVertexBuffer(numVtx, g_bd.Layout)
        && (numIdx == 0 || numIdx == bgfx::getAvailTransientIndexBuffer(numIdx, idx32));
}

// ── Public API ────────────────────────────────────────────────────────────────

bool ImGui_ImplBgfx_Init(bgfx::ViewId viewId)
{
    IMGUI_CHECKVERSION();

    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Renderer backend already initialized");

    g_bd.ViewId = viewId;

    const bgfx::RendererType::Enum type = bgfx::getRendererType();
    g_bd.Program = bgfx::createProgram(
        bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_ocornut_imgui"),
        bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_ocornut_imgui"),
        true);

    g_bd.Layout
        .begin()
        .add(bgfx::Attrib::Position,  2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
        .end();

    g_bd.TexSampler = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);

    io.BackendRendererUserData = (void*)&g_bd;
    io.BackendRendererName     = "imgui_impl_bgfx";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    g_bd.Initialized = true;
    return true;
}

void ImGui_ImplBgfx_Shutdown()
{
    if (!g_bd.Initialized)
        return;

    // Destroy any textures ImGui still owns.
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
    {
        if (tex->RefCount == 1 && tex->GetTexID() != ImTextureID_Invalid)
        {
            bgfx::TextureHandle handle = FromImTex(tex->GetTexID());
            if (bgfx::isValid(handle))
                bgfx::destroy(handle);
            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
        }
    }

    if (bgfx::isValid(g_bd.TexSampler)) bgfx::destroy(g_bd.TexSampler);
    if (bgfx::isValid(g_bd.Program))    bgfx::destroy(g_bd.Program);

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName     = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);

    g_bd = ImplBgfxData{};
}

void ImGui_ImplBgfx_NewFrame()
{
    IM_ASSERT(g_bd.Initialized && "ImGui_ImplBgfx_Init() not called");
}

void ImGui_ImplBgfx_SetViewId(bgfx::ViewId viewId)
{
    g_bd.ViewId = viewId;
}

void ImGui_ImplBgfx_RenderDrawData(ImDrawData* drawData)
{
    // Service texture create/update/destroy requests before drawing.
    if (drawData->Textures != nullptr)
        for (ImTextureData* tex : *drawData->Textures)
            if (tex->Status != ImTextureStatus_OK)
                UpdateTexture(tex);

    const int fbWidth  = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
    const int fbHeight = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);
    if (fbWidth <= 0 || fbHeight <= 0)
        return;

    const bgfx::ViewId viewId = g_bd.ViewId;
    bgfx::setViewName(viewId, "ImGui");
    bgfx::setViewMode(viewId, bgfx::ViewMode::Sequential);

    const bgfx::Caps* caps = bgfx::getCaps();
    {
        float ortho[16];
        const float x = drawData->DisplayPos.x;
        const float y = drawData->DisplayPos.y;
        const float w = drawData->DisplaySize.x;
        const float h = drawData->DisplaySize.y;
        bx::mtxOrtho(ortho, x, x + w, y + h, y, 0.0f, 1000.0f, 0.0f, caps->homogeneousDepth);
        bgfx::setViewTransform(viewId, nullptr, ortho);
        bgfx::setViewRect(viewId, 0, 0, (uint16_t)w, (uint16_t)h);
    }

    const ImVec2 clipPos   = drawData->DisplayPos;
    const ImVec2 clipScale = drawData->FramebufferScale;
    const bool   idx32     = sizeof(ImDrawIdx) == 4;

    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* drawList = drawData->CmdLists[n];
        const uint32_t numVtx = (uint32_t)drawList->VtxBuffer.Size;
        const uint32_t numIdx = (uint32_t)drawList->IdxBuffer.Size;

        if (!CheckAvail(numVtx, numIdx, idx32))
            break;  // out of transient buffer space; skip the rest

        bgfx::TransientVertexBuffer tvb;
        bgfx::TransientIndexBuffer  tib;
        bgfx::allocTransientVertexBuffer(&tvb, numVtx, g_bd.Layout);
        bgfx::allocTransientIndexBuffer(&tib, numIdx, idx32);

        bx::memCopy(tvb.data, drawList->VtxBuffer.Data, numVtx * sizeof(ImDrawVert));
        bx::memCopy(tib.data, drawList->IdxBuffer.Data, numIdx * sizeof(ImDrawIdx));

        bgfx::Encoder* encoder = bgfx::begin();

        for (const ImDrawCmd& cmd : drawList->CmdBuffer)
        {
            if (cmd.UserCallback != nullptr)
            {
                cmd.UserCallback(drawList, &cmd);
                continue;
            }
            if (cmd.ElemCount == 0)
                continue;

            // Project scissor rect into framebuffer space.
            ImVec4 clip;
            clip.x = (cmd.ClipRect.x - clipPos.x) * clipScale.x;
            clip.y = (cmd.ClipRect.y - clipPos.y) * clipScale.y;
            clip.z = (cmd.ClipRect.z - clipPos.x) * clipScale.x;
            clip.w = (cmd.ClipRect.w - clipPos.y) * clipScale.y;
            if (clip.x >= fbWidth || clip.y >= fbHeight || clip.z < 0.0f || clip.w < 0.0f)
                continue;

            const uint16_t xx = (uint16_t)bx::max(clip.x, 0.0f);
            const uint16_t yy = (uint16_t)bx::max(clip.y, 0.0f);
            encoder->setScissor(xx, yy,
                (uint16_t)(bx::min(clip.z, 65535.0f) - xx),
                (uint16_t)(bx::min(clip.w, 65535.0f) - yy));

            const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA
                | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
            encoder->setState(state);

            bgfx::TextureHandle th = BGFX_INVALID_HANDLE;
            const ImTextureID texId = cmd.GetTexID();
            if (texId != ImTextureID_Invalid)
                th = FromImTex(texId);
            encoder->setTexture(0, g_bd.TexSampler, th);

            encoder->setVertexBuffer(0, &tvb, cmd.VtxOffset, numVtx);
            encoder->setIndexBuffer(&tib, cmd.IdxOffset, cmd.ElemCount);
            encoder->submit(viewId, g_bd.Program);
        }

        bgfx::end(encoder);
    }
}
