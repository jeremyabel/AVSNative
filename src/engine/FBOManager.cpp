#include "FBOManager.h"

void FBOManager::CreateSlot(FBOSlot& Slot, uint16_t Width, uint16_t Height)
{
    Slot.Texture = bgfx::createTexture2D(Width, Height, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    Slot.Fbo = bgfx::createFrameBuffer(1, &Slot.Texture, false);
}

void FBOManager::DestroySlot(FBOSlot& Slot)
{
    if (bgfx::isValid(Slot.Fbo))
        bgfx::destroy(Slot.Fbo);

    if (bgfx::isValid(Slot.Texture))
        bgfx::destroy(Slot.Texture);

    Slot.Fbo = BGFX_INVALID_HANDLE;
    Slot.Texture = BGFX_INVALID_HANDLE;
}

void FBOManager::Init(uint16_t Width, uint16_t Height)
{
    this->Width = Width;
    this->Height = Height;
    Current = 0;

    for (int PingPongIdx = 0; PingPongIdx < 2; PingPongIdx++)
    {
        CreateSlot(PingPongBuffers[PingPongIdx], Width, Height);
    }

    for (int ScratchIdx = 0; ScratchIdx < SCRATCH_BUFFER_COUNT; ScratchIdx++)
    {
        CreateSlot(ScratchBuffers[ScratchIdx], Width, Height);
    }
}

void FBOManager::Resize(uint16_t Width, uint16_t Height)
{
    if (Width == this->Width && Height == this->Height)
        return;

    this->Width = Width;
    this->Height = Height;

    for (int PingPongIdx = 0; PingPongIdx < 2; PingPongIdx++)
    {
        DestroySlot(PingPongBuffers[PingPongIdx]);
        CreateSlot(PingPongBuffers[PingPongIdx], Width, Height);
    }

    for (int ScratchIdx = 0; ScratchIdx < SCRATCH_BUFFER_COUNT; ScratchIdx++)
    {
        DestroySlot(ScratchBuffers[ScratchIdx]);
        CreateSlot(ScratchBuffers[ScratchIdx], Width, Height);
    }
}

void FBOManager::Destroy()
{
    for (int PingPongIdx = 0; PingPongIdx < 2; PingPongIdx++)
    {
        DestroySlot(PingPongBuffers[PingPongIdx]);
    }

    for (int ScratchIdx = 0; ScratchIdx < SCRATCH_BUFFER_COUNT; ScratchIdx++)
    {   
        DestroySlot(ScratchBuffers[ScratchIdx]);
    }
}

FBOSlot& FBOManager::GetCurrent()
{
    return PingPongBuffers[Current];
}

FBOSlot& FBOManager::GetNext()
{
    return PingPongBuffers[1 - Current];
}

void FBOManager::Swap()
{
    Current = 1 - Current;
}

FBOSlot& FBOManager::GetScratch(int Index)
{
    return ScratchBuffers[Index % SCRATCH_BUFFER_COUNT];
}

uint16_t FBOManager::GetWidth() const
{
    return Width;
}

uint16_t FBOManager::GetHeight() const
{
    return Height;
}

// ─── InnerFBOManager ─────────────────────────────────────────────────────────

void InnerFBOManager::Setup(FBOManager* OuterManager, uint16_t W, uint16_t H)
{
    Release();
    Outer  = OuterManager;
    InnerW = W;
    InnerH = H;
    InnerCurrent = 0;
    CreateSlot(Inner[0], W, H);
    CreateSlot(Inner[1], W, H);
}

void InnerFBOManager::Release()
{
    DestroySlot(Inner[0]);
    DestroySlot(Inner[1]);
    Outer = nullptr;
}

FBOSlot& InnerFBOManager::GetCurrent() { return Inner[InnerCurrent]; }
FBOSlot& InnerFBOManager::GetNext()    { return Inner[1 - InnerCurrent]; }
void     InnerFBOManager::Swap()       { InnerCurrent = 1 - InnerCurrent; }

FBOSlot& InnerFBOManager::GetScratch(int Index) { return Outer->GetScratch(Index); }
uint16_t InnerFBOManager::GetWidth()  const     { return InnerW; }
uint16_t InnerFBOManager::GetHeight() const     { return InnerH; }
