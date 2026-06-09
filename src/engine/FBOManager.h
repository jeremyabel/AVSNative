#pragma once

#include <bgfx/bgfx.h>
#include <cstdint>

static constexpr int32_t SCRATCH_BUFFER_COUNT = 8;

struct FBOSlot
{
    bgfx::FrameBufferHandle Fbo = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle Texture = BGFX_INVALID_HANDLE;
};

class FBOManager
{
public:
    virtual ~FBOManager() = default;

    void Init(uint16_t Width, uint16_t Height);
    void Resize(uint16_t Width, uint16_t Height);
    void Destroy();

    virtual FBOSlot& GetCurrent();
    virtual FBOSlot& GetNext();
    virtual void Swap();

    virtual FBOSlot& GetScratch(int Index);

    virtual uint16_t GetWidth() const;
    virtual uint16_t GetHeight() const;

protected:
    void CreateSlot(FBOSlot& Slot, uint16_t Width, uint16_t Height);
    void DestroySlot(FBOSlot& Slot);

    FBOSlot PingPongBuffers[2];
    FBOSlot ScratchBuffers[SCRATCH_BUFFER_COUNT];
    int32_t Current = 0;
    uint16_t Width = 0;
    uint16_t Height = 0;
};

// Isolated ping-pong pair used by EffectList. Scratch and dimensions delegate to an
// outer FBOManager so sub-effects share the same scratch slots as the parent chain.
class InnerFBOManager : public FBOManager
{
public:
    void Setup(FBOManager* Outer, uint16_t W, uint16_t H);
    void Release();

    FBOSlot& GetCurrent() override;
    FBOSlot& GetNext()    override;
    void     Swap()       override;

    FBOSlot& GetScratch(int Index) override;
    uint16_t GetWidth()  const override;
    uint16_t GetHeight() const override;

private:
    FBOSlot      Inner[2];
    int32_t      InnerCurrent = 0;
    FBOManager*  Outer        = nullptr;
    uint16_t     InnerW       = 0;
    uint16_t     InnerH       = 0;
};
